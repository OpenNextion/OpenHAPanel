#include <stdio.h>
#include <string.h>
#include "network_wifi_sta.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <mdns_api.h>
#include "esp_netif.h"
#include "esp_mac.h"
#include "lwip/ip_addr.h"
#include "network_api.h"
#include "network_file.h"

static const char *TAG = "network_wifi_sta";

/*
 * Keep off-channel dwell time short while SoftAP clients are connected.
 * Returning to the home channel between scanned channels gives captive-portal
 * clients time to receive beacons and exchange HTTP/WebSocket traffic.
 */
#define WIFI_SCAN_ACTIVE_MAX_MS 120
#define WIFI_SCAN_HOME_DWELL_MS 50
#define WIFI_SWITCH_CONNECT_TIMEOUT_SECONDS 15

/**
 * @brief Flag indicating whether the Wi-Fi STA wait timeout callback is active.
 *
 * Set to 1 when a wait timeout timer or watchdog is in use during Wi-Fi connection.
 */
int32_t wifi_wait_timeout_callback_flag = 0;

/**
 * @brief Number of seconds to wait before a Wi-Fi STA connection is considered timed out.
 *
 * Used for controlling connection timeout behavior.
 */
int32_t wifi_wait_timeout_callback_times = 0;

/**
 * @brief ESP-NETIF handle for the Wi-Fi STA (station) interface.
 *
 * Created via esp_netif_create_default_wifi_sta() and used for managing IP and DHCP.
 */
static esp_netif_t *wifi_sta_netif = NULL;

/**
 * @brief Indicates whether a Wi-Fi scan is currently in progress.
 *
 * Used to prevent concurrent scans or to track scan state for UI/network reporting.
 */
static int32_t wifi_scan_start = 0;

/** Suppress normal retry handling for the disconnect used to switch STA config. */
static volatile bool s_intentional_disconnect = false;

/**
 * @brief Get the current Wi-Fi scan state.
 *
 * This function returns the internal scan status flag indicating
 * whether a Wi-Fi scan is in progress.
 *
 * @return int32_t Returns 1 if scanning is active, 0 otherwise.
 */
int32_t network_get_if_wifi_scan()
{
    return wifi_scan_start;
}

/**
 * @brief Start a Wi-Fi scan with the given duration.
 *
 * This function uses bounded per-channel timing so that an all-channel scan
 * does not starve clients connected to the device's SoftAP. The scan results
 * will be handled asynchronously by the scan done event handler.
 *
 * @param[in] duration Requested scan duration in seconds. Retained for API
 *                     compatibility and diagnostic logging; safe per-channel
 *                     timing takes precedence while SoftAP is active.
 */
void network_wifi_scan_start(int duration)
{
    wifi_scan_config_t scan_conf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 0,
                .max = WIFI_SCAN_ACTIVE_MAX_MS,
            },
            .passive = 0,
        },
        .home_chan_dwell_time = WIFI_SCAN_HOME_DWELL_MS,
    };
    esp_err_t ret = esp_wifi_scan_start(&scan_conf, false);
    if (ret == ESP_OK) {
        wifi_scan_start = 1;
        ESP_LOGI(TAG, "Wi-Fi scan started: requested_duration=%ds, active_max=%dms, home_dwell=%dms", duration,
                 WIFI_SCAN_ACTIVE_MAX_MS, WIFI_SCAN_HOME_DWELL_MS);
    } else {
        /* Scan failed to start — restore retry immediately */
        network_sta_retry_resume_after_scan();
        ESP_LOGE(TAG, "Failed to start Wi-Fi scan: %s", esp_err_to_name(ret));
    }
}

void network_wifi_scan(int duration)
{
    /* Pause the STA retry loop so esp_wifi_scan_start() does not collide
     * with an in-flight esp_wifi_connect().
     *
     * If pause returns true the STA was actively connecting and a deferred
     * scan has been queued; the disconnect handler will call
     * network_wifi_scan_start() once the STA is confirmed idle.
     *
     * If pause returns false the STA was already idle/connected and we can
     * start scanning immediately. */
    if (network_sta_retry_pause_for_scan(duration)) {
        return;
    }
    network_wifi_scan_start(duration);
}

/**
 * @brief Handler for Wi-Fi STA scan completion event.
 *
 * This function is called when a Wi-Fi scan completes. It:
 * - Checks if scanning was initiated.
 * - Retrieves the number of found access points.
 * - Allocates memory to store AP records.
 * - Fetches the AP records and stops the scan.
 * - Notifies the web server with the scan results.
 *
 * If no APs are found or memory allocation fails, appropriate cleanup is performed.
 *
 * @param[in] arg Pointer to user data (not used).
 * @param[in] event_base The event base, expected to be WIFI_EVENT.
 * @param[in] event_id The event ID, expected to be WIFI_EVENT_SCAN_DONE.
 * @param[in] event_data Pointer to event-specific data (not used).
 */
void network_sta_scan_done_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (wifi_scan_start == 0) {
        return;
    }
    ESP_LOGI(TAG, "network_sta_scan_done_handler done ...");
    wifi_scan_start = 0;

    /* Retrieve scan results BEFORE resuming the retry loop.
     * network_sta_retry_resume_after_scan() may call esp_wifi_connect(),
     * which internally starts a new scan and overwrites the AP list buffer. */
    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    if (apCount == 0) {
        ESP_LOGI(TAG, "Nothing AP found");
        network_sta_retry_resume_after_scan();
        return;
    }
    wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
    if (!ap_list) {
        ESP_LOGI(TAG, "malloc error, ap_list is NULL");
        esp_wifi_clear_ap_list();
        network_sta_retry_resume_after_scan();
        return;
    }
    esp_wifi_scan_get_ap_records(&apCount, ap_list);
    esp_wifi_scan_stop();
    webserver_network_wifi_scan_info_notify(ap_list, apCount);
    free(ap_list);

    /* Resume the STA retry loop that was paused before the scan started.
     * Must be called AFTER scan results are retrieved to avoid overwriting. */
    network_sta_retry_resume_after_scan();
}

/**
 * @brief Apply static IP settings to the Wi-Fi STA interface.
 *
 * This function disables DHCP and manually sets the IP address, netmask,
 * gateway, and both DNS servers (primary and secondary) using the provided
 * configuration.
 *
 * If setting the IP info fails, an error is logged and DNS settings are not applied.
 *
 * @param[in] ip A structure containing the static IP, netmask, gateway, and DNS addresses.
 */
void network_wifi_sta_set_static_ip(network_ip_config_t ip)
{
    esp_netif_dhcpc_stop(wifi_sta_netif);
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
    ip_info.ip.addr = ip.ip;
    ip_info.netmask.addr = ip.netmask;
    ip_info.gw.addr = ip.gateway;

    if (esp_netif_set_ip_info(wifi_sta_netif, &ip_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }
    esp_netif_dns_info_t dns = {0};
    dns.ip.u_addr.ip4.addr = ip.dns1;
    dns.ip.type = IPADDR_TYPE_V4;
    esp_netif_set_dns_info(wifi_sta_netif, ESP_NETIF_DNS_MAIN, &dns);

    dns.ip.u_addr.ip4.addr = ip.dns2;
    dns.ip.type = IPADDR_TYPE_V4;
    esp_netif_set_dns_info(wifi_sta_netif, ESP_NETIF_DNS_BACKUP, &dns);
}

/**
 * @brief Restart the DHCP client on the Wi-Fi STA interface.
 *
 * This function stops and restarts the DHCP client on the Wi-Fi station
 * interface. Useful when changing networks or refreshing IP settings.
 */
void network_wifi_sta_restart_dhcp(void)
{
    esp_netif_dhcpc_stop(wifi_sta_netif);
    esp_netif_dhcpc_start(wifi_sta_netif);
}

/**
 * @brief Stop the DHCP client on the Wi-Fi STA interface.
 *
 * This function stops the DHCP client on the Wi-Fi station
 */
void network_wifi_sta_stop_dhcp(void)
{
    esp_netif_dhcpc_stop(wifi_sta_netif);
}

/**
 * @brief Retrieve the primary DNS server address assigned to the Wi-Fi STA interface.
 *
 * Uses ESP-IDF APIs to get the main DNS server configured by DHCP for the
 * station interface.
 *
 * @return uint32_t The primary DNS server's IPv4 address (in network byte order).
 */
uint32_t network_wifi_sta_get_dns1(void)
{
    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(wifi_sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    return dns_info.ip.u_addr.ip4.addr;
}

/**
 * @brief Retrieve the secondary DNS server address assigned to the Wi-Fi STA interface.
 *
 * Uses ESP-IDF APIs to get the backup DNS server configured by DHCP for the
 * station interface.
 *
 * @return uint32_t The secondary DNS server's IPv4 address (in network byte order).
 */
uint32_t network_wifi_sta_get_dns2(void)
{
    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(wifi_sta_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
    return dns_info.ip.u_addr.ip4.addr;
}

/**
 * @brief Configure and connect to a specified Wi-Fi station (STA).
 *
 * This function performs the following actions:
 * - Initializes a `wifi_config_t` structure with the given connection parameters.
 * - Copies the BSSID (MAC address) and SSID from the input parameter.
 * - Sets the password only if it is at least 8 characters long.
 * - Applies the configuration using ESP-IDF Wi-Fi APIs.
 * - Disconnects any existing connection and attempts to connect to the new network.
 *
 * @param[in] params Pointer to a `network_sta_connect_params_t` structure
 *                   containing the target network’s SSID, password, and BSSID.
 */
esp_err_t network_wifi_sta_connect(network_sta_connect_params_t *params)
{
    if (!params || params->ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_sta_cfg = {.sta = {
                                      .scan_method = WIFI_ALL_CHANNEL_SCAN,
                                      .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                                  }};
    memcpy((char *)wifi_sta_cfg.sta.bssid, (char *)params->bssid, 6);
    strcpy((char *)wifi_sta_cfg.sta.ssid, (char *)params->ssid);
    if (strlen((char *)params->password) >= 8) {
        strcpy((char *)wifi_sta_cfg.sta.password, (char *)params->password);
    }

    ESP_LOGI(TAG, "Applying STA config for SSID: %s", wifi_sta_cfg.sta.ssid);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply STA config: %s", esp_err_to_name(err));
        return err;
    }

    /* The disconnect event is asynchronous. The new connection is started
     * here exactly once; its later intentional disconnect event is consumed by
     * network_sta_disconnected_handler() instead of triggering another call. */
    s_intentional_disconnect = true;
    esp_err_t disconnect_err = esp_wifi_disconnect();
    if (disconnect_err != ESP_OK) {
        s_intentional_disconnect = false;
        ESP_LOGW(TAG, "Failed to disconnect previous STA attempt: %s", esp_err_to_name(disconnect_err));
    }

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start STA connection: %s", esp_err_to_name(err));
    }
    return err;
}

bool network_wifi_sta_take_intentional_disconnect(void)
{
    if (!s_intentional_disconnect) {
        return false;
    }

    s_intentional_disconnect = false;
    return true;
}

/**
 * @brief Disconnect to a specified Wi-Fi station (STA).
 *
 * This function performs the following actions:
 * - Applies the configuration using ESP-IDF Wi-Fi APIs.
 * - Disconnects any existing connection.
 *
 * @param[in] params void
 */
void network_wifi_sta_disconnect()
{
    wifi_config_t wifi_sta_cfg = {0};

    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_cfg);
    esp_wifi_disconnect();
}

/**
 * @brief Start connecting to a Wi-Fi station (STA) with specified parameters.
 *
 * This function initiates a Wi-Fi STA connection sequence. It performs the following:
 * - Sets a maximum association wait timeout (15 seconds).
 * - If Ethernet is connected, stops the DHCP client on the Wi-Fi STA interface.
 *   Otherwise, restarts DHCP for Wi-Fi STA.
 * - Clears and updates temporary Wi-Fi parameters with the given input.
 * - Initiates the actual connection using those parameters.
 * - Resets connection retry counters and sets the connection start flag.
 *
 * @param[in] params Structure containing the SSID, password, and other parameters
 *                   required for the Wi-Fi STA connection.
 * @return ESP_OK if the attempt was started, otherwise an ESP-IDF error.
 */
esp_err_t network_wifi_sta_start_connect(network_sta_connect_params_t params)
{
    /* Cancel delayed retries belonging to the previous configured WLAN before
     * any asynchronous disconnect/connect event can run. */
    network_reset_sta_retry_state();
    net_state.sta_connect_start_flag = 1;

    /* Bound the complete association attempt; deterministic failures are
     * reported earlier by the disconnect handler. */
    wifi_wait_timeout_callback_times = WIFI_SWITCH_CONNECT_TIMEOUT_SECONDS;
    wifi_wait_timeout_callback_flag = 1;
    /* A newly selected WLAN starts with DHCP. Static addressing can be
     * explicitly configured after the connection succeeds. */
    net_state.sta_ip_mode = IP_MODE_DHCP_AUTO;
    network_wifi_sta_restart_dhcp();
    memset(&net_state.temp_wifi_params, 0, sizeof(network_sta_connect_params_t));
    memcpy(&net_state.temp_wifi_params, &params, sizeof(network_sta_connect_params_t));
    esp_err_t err = network_wifi_sta_connect(&net_state.temp_wifi_params);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue selected STA connection: %s", esp_err_to_name(err));
        wifi_wait_timeout_callback_flag = 0;
        wifi_wait_timeout_callback_times = 0;
        net_state.sta_connect_start_flag = 0;
    }
    return err;
}

/**
 * @brief Apply and persist the selected station IP mode.
 *
 * This function performs the following actions depending on the current station IP mode:
 * - If the mode is DHCP_AUTO, it restarts the DHCP client.
 * - If the mode is STATIC:
 *   - Stores the already validated static IP configuration.
 *   - If connected, applies the static IP configuration.
 *   - If not connected, saves the static IP and notifies the web server.
 *
 * The updated network configuration is saved to persistent storage.
 *
 * @param[in] ip The IP configuration to apply when in static mode.
 */
void network_wifi_sta_apply_ip_mode(network_ip_config_t ip)
{
    if (net_state.sta_ip_mode == IP_MODE_DHCP_AUTO) {
        network_wifi_sta_restart_dhcp();
    }
    else if (net_state.sta_ip_mode == IP_MODE_STATIC) {
        memcpy(&net_state.sta_static_ip, &ip, sizeof(network_ip_config_t));
        if (net_state.sta_connected_flag == 1) {
            network_wifi_sta_set_static_ip(ip);
        }
        else {
            webserver_network_mode_info_notify(NET_MODE_WLAN_ONLY);
        }
    }
    network_save_config_to_file(net_state);
}

/**
 * @brief Create an IPv6 link-local address for the sta network interface.
 *
 * Attempts to create a link-local IPv6 address on the sta network interface
 * (eth_netif). Logs success or failure accordingly.
 */
static void sta_create_ipv6(void)
{
    if (wifi_sta_netif) {
        esp_err_t err = esp_netif_create_ip6_linklocal(wifi_sta_netif);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create IPv6 link-local: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "IPv6 link-local address created");
        }
    }
    else {
        ESP_LOGW(TAG, "wifi_sta_netif is NULL, cannot create IPv6 address");
    }
}

/**
 * @brief sta event handler to handle sta-related events.
 *
 * This handler listens for sta events. When an sta connection
 * is established, it triggers the creation of an IPv6 link-local address.
 *
 * @param arg User-defined argument (unused).
 * @param event_base The event base (should be ETH_EVENT).
 * @param event_id The event ID (e.g., ETHERNET_EVENT_CONNECTED).
 * @param event_data Event-specific data (unused).
 */
static void sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        sta_create_ipv6();
    }
}

esp_netif_t *network_sta_get_netif(void)
{
    return wifi_sta_netif;
}

void network_wifi_sta_update_hostname()
{
    uint8_t esp_mac[6] = {0};
    char name_buf[50] = {0};
    if (!mdns_server_get_hostname(name_buf, sizeof(name_buf))) {
        esp_read_mac(esp_mac, ESP_MAC_WIFI_STA);
        snprintf(name_buf, sizeof(name_buf), DEFAULTS_HOST_NAME "-%02x%02x", esp_mac[4], esp_mac[5]);
    }
    esp_netif_set_hostname(wifi_sta_netif, name_buf);
}

/**
 * @brief Initialize the Wi-Fi station interface and set a unique hostname.
 *
 * This function performs the following steps:
 * - Creates the default Wi-Fi station network interface.
 * - Reads the device's MAC address for the Wi-Fi station.
 * - Constructs a default hostname in the format "nextion-xxxx", where "xxxx"
 *   corresponds to the lowercase last two bytes of the MAC address.
 * - Sets the constructed hostname for the Wi-Fi station interface.
 *
 * This helps uniquely identify the device on the network.
 */
void network_wifi_sta_init(void)
{
    wifi_sta_netif = esp_netif_create_default_wifi_sta();
    network_wifi_sta_update_hostname();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &sta_event_handler, NULL));
}