#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network.h"
#include "network_wifi_ap.h"
#include "network_wifi_sta.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <lwip/etharp.h>

#include "network_api.h"
#include "network_file.h"
#include "lwip/dns.h"
#include "network_captive_portal.h"
#include "network_ping.h"
#include "webserver_client_node.h"
#include "webserver_method_api.h"
static const char *TAG = "network_main";

static TimerHandle_t wifi_timer_handle = NULL;

#define AP_AUTO_STOP_DELAY_MS (10 * 1000)

static TimerHandle_t ap_auto_stop_timer_handle = NULL;
static void network_ap_auto_stop_update(void);

void network_ap_set_user_default(bool default_active)
{
    net_state.ap_config.default_active = default_active;
    network_ap_auto_stop_update();
}

void network_ap_client_connection_changed(void)
{
    network_ap_auto_stop_update();
}

/**
 * @brief Network state structure instance, initialized to zero.
 */
EXT_RAM_BSS_ATTR NetworkState net_state = {0};

/**
 * @brief Flag indicating whether Wi-Fi STA (station mode) has obtained an IP address.
 */
static bool network_sta_got_ip_flag = false;

/**
 * @brief Whether the STA got-IP handler is currently registered.
 */
static bool network_sta_ip_handler_registered = false;

/**
 * @brief Whether the normal STA connected/disconnected handlers are registered.
 */
static bool network_sta_connect_handlers_registered = false;

/**
 * @brief Wi-Fi connection retry count.
 */
int32_t wifi_connect_retry_count = 0;

/**
 * @brief Flag indicating Wi-Fi connection retry attempts exceeded 3 times.
 */
static int32_t wifi_connect_retry_over_3_times_flag = 0;

/**
 * @brief Wait time after Wi-Fi connection retries exceeded 3 times.
 */
static int32_t wifi_connect_retry_over_3_times_wait_time = 0;

static int32_t s_saved_retry_count = 0;
static int32_t s_saved_retry_over_3_flag = 0;
static int32_t s_saved_retry_over_3_wait = 0;
static bool s_retry_paused_for_scan = false;

/** Deferred scan: when true the disconnect handler will start the scan
 *  after confirming STA is fully idle, avoiding radio-state races. */
static bool s_pending_scan = false;
static int  s_pending_scan_duration = 3;

#define WIFI_SWITCH_FAILURE_RETRY_LIMIT 2
#define WIFI_SWITCH_DHCP_TIMEOUT_SECONDS 10

static int s_wifi_switch_last_error = WEB_API_ERROR_OK;
static uint8_t s_wifi_switch_failure_count = 0;
static int s_deferred_wifi_switch_error = WEB_API_ERROR_OK;

/**
 * @brief External flag indicating Wi-Fi wait timeout callback status.
 */
extern int32_t wifi_wait_timeout_callback_flag;

/**
 * @brief External counter for the number of Wi-Fi wait timeout callback occurrences.
 */
extern int32_t wifi_wait_timeout_callback_times;

/**
 * @brief Flag indicating change in ping status.
 */
bool ping_change_flag = false;

/**
 * @brief Get pointer to the global network state structure.
 *
 * @return Pointer to NetworkState instance.
 */
NetworkState *network_get_state(void)
{
    return &net_state;
}

/**
 * @brief Set the STA connection start flag in the global network state.
 *
 * @param flag Boolean value to set the STA connect start flag.
 */
void network_set_sta_connect_start_flag(bool flag)
{
    net_state.sta_connect_start_flag = flag;
}

void network_reset_sta_retry_state(void)
{
    wifi_connect_retry_count = 0;
    wifi_connect_retry_over_3_times_flag = 0;
    wifi_connect_retry_over_3_times_wait_time = 0;
    s_wifi_switch_last_error = WEB_API_ERROR_OK;
    s_wifi_switch_failure_count = 0;
}

bool network_sta_retry_pause_for_scan(int duration)
{
    s_saved_retry_count = wifi_connect_retry_count;
    s_saved_retry_over_3_flag = wifi_connect_retry_over_3_times_flag;
    s_saved_retry_over_3_wait = wifi_connect_retry_over_3_times_wait_time;
    wifi_connect_retry_over_3_times_flag = 0;
    wifi_connect_retry_over_3_times_wait_time = 0;
    s_retry_paused_for_scan = true;
    ESP_LOGI(TAG, "STA retry loop paused for Wi-Fi scan (saved: count=%ld flag=%ld wait=%ld)",
             (long)s_saved_retry_count, (long)s_saved_retry_over_3_flag, (long)s_saved_retry_over_3_wait);

    /* If the STA is connected (retry_count == 0), scanning does not require a
     * disconnect — the ESP32 can scan while associated. */
    if (wifi_connect_retry_count == 0) {
        ESP_LOGI(TAG, "STA is connected, scan will proceed without disconnect");
        return false;
    }

    if (wifi_connect_retry_count <= 3) {
        /* STA is actively connecting (esp_wifi_connect() already in flight).
         * Do NOT force-disconnect — that races with the internal connection
         * state machine and leaves the radio unable to scan. Instead, defer
         * the scan until the connection attempt fails naturally and
         * WIFI_EVENT_STA_DISCONNECTED fires (typically within a few seconds). */
        s_pending_scan = true;
        s_pending_scan_duration = duration;
        ESP_LOGI(TAG, "Scan deferred until natural STA disconnect (count=%ld)",
                 (long)wifi_connect_retry_count);
        return true;
    }

    /* count > 3: STA is idle between timer-driven retries.
     * esp_wifi_disconnect() is a safe no-op here; brief delay then scan. */
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_ERR_WIFI_NOT_STARTED && err != ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGW(TAG, "esp_wifi_disconnect during scan pause: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(150));
    return false;
}

void network_sta_retry_resume_after_scan(void)
{
    if (!s_retry_paused_for_scan) {
        return;
    }
    s_retry_paused_for_scan = false;
    wifi_connect_retry_count = s_saved_retry_count;
    wifi_connect_retry_over_3_times_flag = s_saved_retry_over_3_flag;
    wifi_connect_retry_over_3_times_wait_time = s_saved_retry_over_3_wait;
    if(wifi_connect_retry_over_3_times_flag && wifi_connect_retry_over_3_times_wait_time <= 0) {
        wifi_connect_retry_over_3_times_wait_time = 2;
    }
    if(wifi_connect_retry_count >3)
    {
        /* The disconnect handler normally sets flag=1 after a failed retry,
         * but if a scan pause raced with the timer firing esp_wifi_connect(),
         * the handler was skipped and flag is 0. Re-arm it here. */
        if (!wifi_connect_retry_over_3_times_flag) {
            wifi_connect_retry_over_3_times_flag = 1;
            wifi_connect_retry_over_3_times_wait_time = 2;
        }
        ESP_LOGI(TAG, "STA retry loop resumed: count=%ld wait=%ld",
                 (long)wifi_connect_retry_count, (long)wifi_connect_retry_over_3_times_wait_time);
    }
    else if(wifi_connect_retry_count >0)
    {
        ESP_LOGI(TAG, "STA retry resumed, immediate reconnection (count=%ld)", (long)wifi_connect_retry_count);
        esp_wifi_connect();
    }
}


static int network_wifi_switch_error_from_reason(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_ASSOC_NOT_AUTHED:
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
    case WIFI_REASON_802_1X_AUTH_FAILED:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return WEB_API_ERROR_WIFI_AUTH_FAILED;
    default:
        return WEB_API_ERROR_WIFI_CONNECT_FAILED;
    }
}

static void network_wifi_switch_finish_failure(int error)
{
    wifi_wait_timeout_callback_flag = 0;
    wifi_wait_timeout_callback_times = 0;
    net_state.sta_connect_start_flag = 0;
    network_reset_sta_retry_state();

    if (net_state.wifi_params.ssid[0] != 0) {
        /* A management-page request may be using the STA connection that was
         * just interrupted. Restore it before replying so the existing socket
         * has a chance to survive the short link outage. */
        s_deferred_wifi_switch_error = error;
        if (network_wifi_sta_connect(&net_state.wifi_params) != ESP_OK) {
            s_deferred_wifi_switch_error = WEB_API_ERROR_OK;
            webserver_network_wifi_connect_result_notify(error);
        }
    }
    else {
        webserver_network_wifi_connect_result_notify(error);
    }
}

/**
 * @brief Check if the given IP address conflicts with any device on the network.
 *
 * This function sends an ARP query to detect if the target IP is already in use.
 *
 * @param target_ip The IP address to check for conflict.
 *
 * @return
 * -  1 if IP conflict detected
 * -  0 if no conflict detected
 * - -1 if invalid parameters (e.g., no network interface or invalid IP)
 */
int network_check_ip_conflict(ip4_addr_t target_ip)
{
    struct netif *netif = netif_default;
    struct eth_addr *found_mac = {0};
    const ip4_addr_t *found_ip = {0};

    if (netif == NULL || ip4_addr_isany_val(target_ip)) {
        ESP_LOGW(TAG, "Invalid netif or target IP");
        return -1;
    }

    ESP_LOGI(TAG, "Checking IP conflict for %s", ip4addr_ntoa(&target_ip));
    if (etharp_query(netif, &target_ip, NULL) != ERR_OK) {
        ESP_LOGW(TAG, "etharp_query failed");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (etharp_find_addr(netif, &target_ip, &found_mac, &found_ip) >= 0) {
        if (memcmp(found_mac->addr, "\0\0\0\0\0\0", ETH_HWADDR_LEN) != 0) {
            ESP_LOGW(TAG, "IP conflict detected");
            return 1;
        }
    }

    ESP_LOGW(TAG, "No IP conflict detected");
    return 0;
}

/**
 * @brief Update the IP configuration structure based on the received IP event.
 *
 * This function extracts IP, netmask, gateway, and DNS information from the
 * event and fills it into the provided `network_ip_config_t` structure.
 *
 * @param ip_cfg Pointer to the IP config structure to update.
 * @param dhcp_ipinfo Pointer to the IP event structure containing new IP info.
 */
static void update_ip_info(network_ip_config_t *ip_cfg, const esp_netif_ip_info_t *dhcp_ipinfo)
{
    ip_cfg->ip = dhcp_ipinfo->ip.addr;
    ip_cfg->netmask = dhcp_ipinfo->netmask.addr;
    ip_cfg->gateway = dhcp_ipinfo->gw.addr;
    ip_cfg->dns1 = network_wifi_sta_get_dns1();
    ip_cfg->dns2 = network_wifi_sta_get_dns2();
}

/**
 * @brief Record the active STA address and notify dependent services.
 *
 * @param event Pointer to the IP event containing newly obtained IP.
 */
static void network_get_ip_handler_by_flag(const ip_event_got_ip_t *event)
{
    update_ip_info(&net_state.current_ip, &event->ip_info);
    if (net_state.sta_ip_mode == IP_MODE_STATIC) {
        net_state.current_ip.dns2 = net_state.sta_static_ip.dns2;
    }

    if (net_state.sta_connect_start_flag) {
        net_state.sta_connect_start_flag = 0;
        wifi_wait_timeout_callback_flag = 0;
        wifi_wait_timeout_callback_times = 0;
        network_reset_sta_retry_state();
        memcpy(&net_state.wifi_params, &net_state.temp_wifi_params, sizeof(network_sta_connect_params_t));
        network_save_config_to_file(net_state);
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_OK);
    }
    else if (s_deferred_wifi_switch_error != WEB_API_ERROR_OK) {
        const int deferred_error = s_deferred_wifi_switch_error;
        s_deferred_wifi_switch_error = WEB_API_ERROR_OK;
        webserver_network_wifi_connect_result_notify(deferred_error);
    }

    if (!network_sta_got_ip_flag) {
        network_sta_got_ip_flag = true;
        webserver_network_mode_info_notify(NET_MODE_WLAN_ONLY);
        network_ap_auto_stop_update();
    }
    if (net_state.ap_config.active && net_state.ap_config.internet_connect) {
        network_wifi_ap_refresh_uplink_state();
    }
    network_start_ping_task();
}

/**
 * @brief Event handler for successful Wi-Fi STA connection.
 *
 * This handler is called when the device successfully connects to a Wi-Fi network
 * (but not yet obtains an IP address). It updates the internal connection state,
 * clears connection retry counters, and starts the bounded DHCP wait when
 * `sta_connect_start_flag` was set. Credentials are persisted only after IP acquisition.
 *
 * @param arg Unused user argument.
 * @param event_base The base of the event (e.g., WIFI_EVENT).
 * @param event_id The event ID (e.g., WIFI_EVENT_STA_CONNECTED).
 * @param event_data Additional event-specific data (unused).
 */
static void network_sta_connected_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // notification_manage_wifi_state_notify_tigger(true);
    ESP_LOGI(TAG, "STA Connecting Success");
    /* Avoid leaving a stale suppression marker if the driver had no previous
     * connection and therefore emitted no disconnect event. */
    (void)network_wifi_sta_take_intentional_disconnect();
    net_state.sta_connected_flag = 1;
    if (net_state.sta_ip_mode == IP_MODE_STATIC) {
        network_wifi_sta_set_static_ip(net_state.sta_static_ip);
    }

    wifi_connect_retry_over_3_times_flag = 0;
    wifi_connect_retry_over_3_times_wait_time = 0;
    wifi_connect_retry_count = 0;
    if (net_state.sta_connect_start_flag) {
        s_deferred_wifi_switch_error = WEB_API_ERROR_OK;
        s_wifi_switch_last_error = WEB_API_ERROR_OK;
        s_wifi_switch_failure_count = 0;
        wifi_wait_timeout_callback_times = WIFI_SWITCH_DHCP_TIMEOUT_SECONDS;
        wifi_wait_timeout_callback_flag = 1;
    }
}

/**
 * @brief Event handler for Wi-Fi STA disconnection.
 *
 * This handler is triggered when the device is disconnected from the Wi-Fi network.
 * It clears STA IP flags, resets or increments retry counters, and handles automatic
 * reconnection logic. If the connection fails repeatedly, the handler sets up
 * a retry delay mechanism.
 *
 * @param arg Unused user argument.
 * @param event_base The base of the event (e.g., WIFI_EVENT).
 * @param event_id The event ID (e.g., WIFI_EVENT_STA_DISCONNECTED).
 * @param event_data Additional event-specific data (unused).
 */
static void network_sta_disconnected_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "STA Connecting Disconnect");
    network_sta_got_ip_flag = false;
    network_ap_auto_stop_update();
    if (net_state.sta_connected_flag) {
        // notification_manage_wifi_state_notify_tigger(false);
        net_state.sta_connected_flag = 0;
        if (!net_state.eth_connected) {
            net_state.internet_connected = 0;
        }
        webserver_network_mode_info_notify(NET_MODE_WLAN_ONLY);
    }
    if (net_state.wifi_params.ssid[0] != 0 && (!net_state.eth_connected || net_state.sta_connect_start_flag)) {
        network_stop_ping_task();
    }

    /* Switching WLANs deliberately disconnects the previous attempt and has
     * already started the new one. Do not mistake that event for a failure or
     * issue a duplicate esp_wifi_connect(). */
    bool intentional_disconnect = network_wifi_sta_take_intentional_disconnect();
    wifi_event_sta_disconnected_t *disconnect_event = event_data;
    if (intentional_disconnect && disconnect_event &&
        (disconnect_event->reason == WIFI_REASON_ASSOC_LEAVE ||
         disconnect_event->reason == WIFI_REASON_AUTH_LEAVE)) {
        ESP_LOGI(TAG,
                 "Ignoring intentional STA disconnect during Wi-Fi switch (reason=%u)",
                 disconnect_event->reason);
        return;
    }
    if (s_retry_paused_for_scan) {
        ESP_LOGI(TAG, "Disconnect while scan is pending, skipping retry");
        if (s_pending_scan) {
            s_pending_scan = false;
            int duration = s_pending_scan_duration;
            ESP_LOGI(TAG, "STA confirmed idle, starting deferred Wi-Fi scan (duration=%ds)", duration);
            network_wifi_scan_start(duration);
        }
        return;
    }
    if (intentional_disconnect) {
        ESP_LOGW(TAG,
                 "Expected intentional STA disconnect but received reason=%u; continuing normal retry",
                 disconnect_event ? disconnect_event->reason : 0);
    }

    if (net_state.sta_connect_start_flag) {
        const int switch_error = network_wifi_switch_error_from_reason(disconnect_event ? disconnect_event->reason : 0);
        if (switch_error == s_wifi_switch_last_error) {
            s_wifi_switch_failure_count++;
        }
        else {
            s_wifi_switch_last_error = switch_error;
            s_wifi_switch_failure_count = 1;
        }

        if (s_wifi_switch_failure_count >= WIFI_SWITCH_FAILURE_RETRY_LIMIT) {
            ESP_LOGW(TAG,
                     "Selected Wi-Fi failed after %u attempts (reason=%u, error=%d)",
                     (unsigned)s_wifi_switch_failure_count,
                     disconnect_event ? disconnect_event->reason : 0,
                     switch_error);
            network_wifi_switch_finish_failure(switch_error);
            return;
        }
    }

    wifi_connect_retry_count++;
    if (wifi_connect_retry_count >= 13) {
        wifi_connect_retry_count = 13;
    }
    wifi_connect_retry_over_3_times_wait_time = wifi_connect_retry_count - 3 + 2;
    if (wifi_connect_retry_count <= 3 && (!net_state.eth_connected || net_state.sta_connect_start_flag)) {
        esp_wifi_connect();
    }
    else {
        wifi_connect_retry_over_3_times_flag = 1;
        /* WiFi 持续连不上：开 AP 作为配网兜底，让用户始终能进控制台 */
        if (!network_wifi_get_ap_enabled()) {
            ESP_LOGW(TAG, "STA connect failed repeatedly, enable provisioning AP");
            esp_err_t ap_err = network_wifi_ap_handler_set_mode(true,
                                                                net_state.ap_params.ssid,
                                                                net_state.ap_params.password,
                                                                net_state.ap_config.internet_connect,
                                                                net_state.ap_config.hide_ssid,
                                                                net_state.ap_config.channel);
            if (ap_err == ESP_OK) {
                net_state.ap_config.active = true;
            }
            else {
                ESP_LOGE(TAG, "Failed to enable provisioning AP: %s", esp_err_to_name(ap_err));
            }
        }
    }
    ESP_LOGI(TAG, "disconnected . ...");
}

/**
 * @brief Event handler for when the Wi-Fi STA interface gets an IP address.
 *
 * This handler is called when the device receives an IP address after a successful
 * Wi-Fi connection. It delegates IP handling logic to `network_get_ip_handler_by_flag()`.
 *
 * @param arg Unused user argument.
 * @param event_base The base of the event (e.g., IP_EVENT).
 * @param event_id The event ID (e.g., IP_EVENT_STA_GOT_IP).
 * @param event_data Pointer to the IP event data (ip_event_got_ip_t).
 */
static void network_sta_get_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Sta get ip");
    network_get_ip_handler_by_flag((ip_event_got_ip_t *)event_data);
}

/**
 * @brief Event handler triggered when a device connects to the SoftAP.
 *
 * This function is called when a station connects to the device's AP interface.
 * It clears Wi-Fi retry flags if no STA SSID is configured and no Ethernet is connected.
 * It also stops any ongoing product testing Wi-Fi operations and notifies the system
 * that the device is currently operating in AP-only mode.
 *
 * @param arg Unused user argument.
 * @param event_base The base of the event (e.g., WIFI_EVENT).
 * @param event_id The event ID (e.g., WIFI_EVENT_AP_STACONNECTED).
 * @param event_data Additional event-specific data (unused).
 */
static void network_ap_start_connected_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                               void *event_data)
{
    wifi_event_ap_staconnected_t *event = event_data;
    ESP_LOGI(TAG, "AP STA connected, MAC=" MACSTR ", AID=%d", MAC2STR(event->mac), event->aid);

    if (net_state.wifi_params.ssid[0] == 0 && !net_state.eth_connected) {
        wifi_connect_retry_over_3_times_flag = 0;
        wifi_connect_retry_over_3_times_wait_time = 0;
    }
    // product_test_wifi_stop();
    webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
}

/**
 * @brief Event handler triggered when a device disconnects from the SoftAP.
 *
 * This function is called when a station disconnects from the AP interface.
 * It updates the system status to reflect that the device is still in AP-only mode.
 *
 * @param arg Unused user argument.
 * @param event_base The base of the event (e.g., WIFI_EVENT).
 * @param event_id The event ID (e.g., WIFI_EVENT_AP_STADISCONNECTED).
 * @param event_data Additional event-specific data (unused).
 */
static void network_ap_start_disconnected_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                                  void *event_data)
{
    wifi_event_ap_stadisconnected_t *event = event_data;
    ESP_LOGW(TAG, "AP STA disconnected, MAC=" MACSTR ", AID=%d, reason=%d", MAC2STR(event->mac), event->aid,
             event->reason);
    webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
}

/**
 * @brief Event handler for when the SoftAP interface is assigned an IP address.
 *
 * This function updates the current AP IP configuration in the system state. If the
 * SoftAP gateway conflicts with the current STA or Ethernet gateway, it reassigns
 * the AP gateway to `192.168.10.1` to avoid conflicts. It also notifies the system
 * of the AP-only network mode.
 *
 * @param arg Unused user argument.
 * @param event_base The base of the event (e.g., IP_EVENT).
 * @param event_id The event ID (e.g., IP_EVENT_AP_STAIPASSIGNED).
 * @param event_data Pointer to the IP event data (ip_event_got_ip_t).
 */
static void network_ap_get_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
}

/**
 * @brief Register or unregister the STA IP event handler based on a flag.
 *
 * This function controls the registration of the IP_EVENT_STA_GOT_IP event handler.
 *
 * @param flag If non-zero, registers the handler; if zero, unregisters it.
 */
void network_sta_get_ip_event_register_by_flag(int flag)
{
    if (flag && !network_sta_ip_handler_registered) {
        esp_err_t ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_sta_get_ip_handler, NULL);
        if (ret == ESP_OK) {
            network_sta_ip_handler_registered = true;
        }
        else {
            ESP_LOGE(TAG, "Failed to register STA got-IP handler: %s", esp_err_to_name(ret));
        }
    }
    else if (!flag && network_sta_ip_handler_registered) {
        esp_err_t ret = esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_sta_get_ip_handler);
        if (ret == ESP_OK) {
            network_sta_ip_handler_registered = false;
        }
        else {
            ESP_LOGE(TAG, "Failed to unregister STA got-IP handler: %s", esp_err_to_name(ret));
        }
    }
}

/**
 * @brief Register or unregister STA connection event handlers based on a flag.
 *
 * This function controls the registration of Wi-Fi station connection and disconnection event handlers.
 *
 * @param flag If non-zero, registers the handlers; if zero, unregisters them.
 */
void network_sta_connect_event_register_by_flag(int flag)
{
    if (flag && !network_sta_connect_handlers_registered) {
        esp_err_t disconnected_ret =
            esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &network_sta_disconnected_handler, NULL);
        esp_err_t connected_ret =
            esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &network_sta_connected_handler, NULL);
        if (disconnected_ret == ESP_OK && connected_ret == ESP_OK) {
            network_sta_connect_handlers_registered = true;
        }
        else {
            if (disconnected_ret == ESP_OK) {
                esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                             &network_sta_disconnected_handler);
            }
            if (connected_ret == ESP_OK) {
                esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &network_sta_connected_handler);
            }
            ESP_LOGE(TAG, "Failed to register STA handlers: disconnected=%s connected=%s",
                     esp_err_to_name(disconnected_ret), esp_err_to_name(connected_ret));
        }
    }
    else if (!flag && network_sta_connect_handlers_registered) {
        esp_err_t disconnected_ret =
            esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &network_sta_disconnected_handler);
        esp_err_t connected_ret =
            esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &network_sta_connected_handler);
        if (disconnected_ret == ESP_OK && connected_ret == ESP_OK) {
            network_sta_connect_handlers_registered = false;
        }
        else {
            ESP_LOGE(TAG, "Failed to unregister STA handlers: disconnected=%s connected=%s",
                     esp_err_to_name(disconnected_ret), esp_err_to_name(connected_ret));
        }
    }
}

static bool network_ap_auto_stop_allowed(void)
{
    return !net_state.ap_config.default_active &&
           network_sta_got_ip_flag &&
           net_state.wifi_params.ssid[0] != '\0' &&
           net_state.ap_config.active &&
           network_wifi_get_ap_enabled() &&
           webserver_client_get_ap_count() == 0;
}

static void network_ap_auto_stop_update(void)
{
    if (!ap_auto_stop_timer_handle) {
        return;
    }

    if (network_ap_auto_stop_allowed()) {
        if (xTimerIsTimerActive(ap_auto_stop_timer_handle) == pdFALSE) {
            ESP_LOGI(TAG, "No AP web client connected, stopping AP in 10 seconds");
            xTimerStart(ap_auto_stop_timer_handle, 0);
        }
    }
    else if (xTimerIsTimerActive(ap_auto_stop_timer_handle) != pdFALSE) {
        ESP_LOGI(TAG, "AP auto-stop cancelled");
        xTimerStop(ap_auto_stop_timer_handle, 0);
    }
}

static void ap_auto_stop_timer_callback(TimerHandle_t xTimer)
{
    if (!network_ap_auto_stop_allowed()) {
        return;
    }

    ESP_LOGI(TAG, "No AP web client connected for 10 seconds, stopping AP");
    network_wifi_ap_set_active(false);
}

/**
 * @brief Timer callback for managing Wi-Fi connection retry and timeout behavior.
 *
 * This function is triggered periodically by a FreeRTOS software timer.
 *
 * @param xTimer The handle of the timer that triggered the callback.
 */
static void wifi_connect_callback(TimerHandle_t xTimer)
{
    /* Wi-Fi connection timeout callback */
    if (wifi_wait_timeout_callback_flag && --wifi_wait_timeout_callback_times <= 0) {
        ESP_LOGI(TAG, "Wi-Fi connection timeout callback");
        network_wifi_switch_finish_failure(WEB_API_ERROR_WIFI_CONNECT_FAILED);
    }
    /* after the Wi-Fi connection failed more than 3 times.
     * Skip the retry step while a scan is in progress so the two do not
     * contend for the radio. The saved state will be restored when the
     * scan completes. */
    if (wifi_connect_retry_over_3_times_flag && !s_retry_paused_for_scan) {
        if (--wifi_connect_retry_over_3_times_wait_time <= 0) {
            wifi_connect_retry_over_3_times_flag = 0;
            wifi_connect_retry_over_3_times_wait_time = 0;
            esp_wifi_connect();
        }
    }
    if (ping_change_flag) {
        ESP_LOGW(TAG, "ping_change_flag callback");
        ping_change_flag = 0;
        network_start_ping_task();
    }
}

/**
 * @brief Register all necessary network-related event handlers.
 *
 * This function sets up event callbacks for Wi-Fi (STA, AP, scan) and IP events.
 */
static void network_event_handler(void)
{
    /* STA event */
    network_sta_connect_event_register_by_flag(true);
    network_sta_get_ip_event_register_by_flag(true);

    /* AP event */
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &network_ap_start_connected_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &network_ap_start_disconnected_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &network_ap_get_ip_handler, NULL);

    /* Wi-Fi scan event */
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &network_sta_scan_done_handler, NULL);
}

/**
 * @brief Initialize the network components and state.
 *
 * This function performs comprehensive initialization of the networking stack,
 * including Wi-Fi setup, event handlers, timers, captive portal,
 * and network state configuration.
 */
void network_init()
{
    /* Network event handler */
    network_event_handler();

    /* Wi-Fi init */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    network_wifi_sta_init();
    network_wifi_ap_init();

    /* Apply a project-owned SSID before Wi-Fi starts so the ESP-IDF default
     * ESP_xxxxxx SoftAP is never advertised during early boot. Persisted AP
     * settings are loaded and applied below when provisioning is required. */
    network_wifi_ap_get_name((char *)net_state.ap_params.ssid, sizeof(net_state.ap_params.ssid));
    esp_err_t initial_ap_err = network_wifi_ap_handler_set_mode(true,
                                                                net_state.ap_params.ssid,
                                                                net_state.ap_params.password,
                                                                false,
                                                                false,
                                                                0);
    if (initial_ap_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply initial provisioning AP config: %s", esp_err_to_name(initial_ap_err));
    }
    esp_wifi_start();

    /* Wi-Fi connect timer */
    wifi_timer_handle = xTimerCreate("wifi_timer_handle", pdMS_TO_TICKS(1000), pdTRUE, 0, wifi_connect_callback);
    xTimerStart(wifi_timer_handle, 100);

    ap_auto_stop_timer_handle =
        xTimerCreate("ap_auto_stop_timer", pdMS_TO_TICKS(AP_AUTO_STOP_DELAY_MS), pdFALSE, 0,
                     ap_auto_stop_timer_callback);

    /* Load network state from file */
    network_load_and_apply_config();

    /* Captive portal */
    captive_portal_init();
}