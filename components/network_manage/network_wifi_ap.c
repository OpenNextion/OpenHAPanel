#include <string.h>
#include <stdlib.h>
#include "network_wifi_ap.h"
#include <network_wifi_sta.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/opt.h"
#include "lwip/ip_addr.h"
#include "lwip/lwip_napt.h"
#include "lwip/netif.h"
#include "dhcpserver/dhcpserver.h"
#include "network_api.h"
#include "network.h"
#include "network_file.h"

static const char *TAG = "network_wifi_ap";

/**
 * @brief Minimum length required for a valid WiFi password.
 */
#define MIN_PASSWORD_LENGTH 8

/**
 * @brief Default static DNS server IP address used by the bridge.
 */
#define CONFIG_BRIDGE_STATIC_DNS_SERVER_MAIN "8.8.8.8"

/**
 * @brief Flag indicating whether the Access Point (AP) is currently started/enabled.
 */
static bool ap_start_flag = true;

/**
 * @brief Network interface handle for the WiFi Access Point.
 */
static esp_netif_t *wifi_ap_netif = NULL;

/**
 * @brief Tracks the previous state of the AP internet connection enable flag.
 */
static int old_internet_connection_flag = 0;

static uint32_t s_napt_bound_ap_ip = 0;
static bool s_napt_enabled = false;

/**
 * @brief Enable or disable internet connection sharing on the Access Point (AP).
 *
 * This function updates the AP internet connection state. If the state changes,
 * it deauthenticates all connected stations, attempts to enable/disable internet
 * connection sharing, updates the configuration, and notifies the webserver.
 *
 * @param enable true to enable internet connection sharing, false to disable.
 *
 * @return 0 on success, -1 on failure.
 */
int network_enable_ap_internet_connection(bool enable)
{
    if (net_state.ap_config.internet_connect == enable) {
        return 0;
    }
    esp_wifi_deauth_sta(0);
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_err_t ret = network_wifi_ap_internet_connection_enable(enable);
    if (ret != 0) {
        webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
        return -1;
    }

    net_state.ap_config.internet_connect = enable;

    network_save_config_to_file(net_state);
    webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
    return 0;
}

/**
 * @brief Start or stop the WiFi Access Point with specified parameters.
 *
 * This function sets the AP active state, copies connection parameters, applies
 * the AP mode settings (including internet connection enable flag), saves configuration,
 * and notifies the webserver of the network mode.
 *
 */
int network_wifi_ap_set_password(const uint8_t *password)
{
    if (password == NULL || password[0] == '\0') {
        return -1;
    }
    // If the password is the same as the password, it is not saved
    if (strcmp((char *)net_state.ap_params.password, (char *)password) == 0) {
        return 0;
    }
    strlcpy((char *)net_state.ap_params.password, (const char *)password, sizeof(net_state.ap_params.password));
    network_wifi_ap_handler_set_mode(net_state.ap_config.active, net_state.ap_params.ssid, net_state.ap_params.password,
                                     net_state.ap_config.internet_connect, net_state.ap_config.hide_ssid,
                                     net_state.ap_config.channel);
    network_save_config_to_file(net_state);
    webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
    return 0;
}

/**
 * @brief Start or stop the Wi-Fi Access Point with specified parameters.
 *
 * This function sets the AP active state, copies connection parameters, applies
 * the AP mode settings (including internet connection enable flag), saves configuration,
 * and notifies the webserver of the network mode.
 *
 */
int network_wifi_ap_set_ssid(const char *ssid)
{
    if (ssid == NULL || ssid[0] == '\0') {
        return -1;
    }
    // If the ssid is the same as the ssid, it is not saved
    if (strcmp((char *)net_state.ap_params.ssid, ssid) == 0) {
        return 0;
    }
    strlcpy((char *)net_state.ap_params.ssid, (const char *)ssid, sizeof(net_state.ap_params.ssid));
    network_wifi_ap_handler_set_mode(net_state.ap_config.active, net_state.ap_params.ssid, net_state.ap_params.password,
                                     net_state.ap_config.internet_connect, net_state.ap_config.hide_ssid,
                                     net_state.ap_config.channel);
    network_save_config_to_file(net_state);
    webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
    return 0;
}

/**
 * @brief Set the active state of the WiFi Access Point.
 *
 * If the current AP active state and the AP enabled status are the same as the
 * requested state, no action is taken. Otherwise, the function enables or disables
 * the AP connection accordingly, manages internet connection sharing if enabled,
 * saves the configuration, and sends a network info notification.
 *
 * @param enable true to activate the AP, false to deactivate.
 */
void network_wifi_ap_set_active(bool enable)
{
    net_state.ap_config.active = enable;
    network_wifi_ap_handler_set_mode(net_state.ap_config.active, net_state.ap_params.ssid, net_state.ap_params.password,
                                     net_state.ap_config.internet_connect, net_state.ap_config.hide_ssid,
                                     net_state.ap_config.channel);
    network_save_config_to_file(net_state);
    webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
}

static bool network_ipv4_netmask_is_valid(uint32_t netmask_addr)
{
    uint32_t mask = ntohl(netmask_addr);
    uint32_t inv = ~mask;

    /*
     * Valid IPv4 netmask must be continuous 1s followed by continuous 0s.
     * Examples:
     * /24: 11111111.11111111.11111111.00000000
     * /25: 11111111.11111111.11111111.10000000
     */
    return ((inv & (inv + 1U)) == 0U);
}

static bool network_ipv4_host_is_usable_for_ap(uint32_t ip_addr, uint32_t netmask_addr)
{
    uint32_t ip = ntohl(ip_addr);
    uint32_t mask = ntohl(netmask_addr);
    uint32_t host_mask = ~mask;

    /*
     * For SoftAP + DHCP server, /31 and /32 are not useful because
     * there is no normal client address range.
     */
    if (host_mask < 3U) {
        return false;
    }

    uint32_t host_part = ip & host_mask;

    /*
     * Reject network address and broadcast address.
     *
     * Example:
     * 192.168.42.0/24   -> network address
     * 192.168.42.255/24 -> broadcast address
     */
    return (host_part != 0U) && (host_part != host_mask);
}

/**
 * @brief Set Wi-Fi SoftAP IP/gateway address and subnet mask.
 *
 * This function stops the DHCP server, updates AP IP, gateway and netmask,
 * configures DHCP DNS offer option, then restarts the DHCP server.
 *
 * @param[in] gw_addr       New AP IP/gateway address in network byte order.
 * @param[in] netmask_addr  New subnet mask in network byte order.
 *
 * @return ESP_OK on success, otherwise ESP-IDF error code.
 */
esp_err_t network_wifi_ap_set_gateway_netmask(uint32_t gw_addr, uint32_t netmask_addr)
{
    esp_err_t ret = ESP_OK;
    esp_err_t start_ret = ESP_OK;
    esp_netif_ip_info_t old_ip_info = {0};
    esp_netif_ip_info_t new_ip_info = {0};

    if (wifi_ap_netif == NULL) {
        ESP_LOGE(TAG, "wifi_ap_netif is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    if (!network_ipv4_netmask_is_valid(netmask_addr)) {
        esp_ip4_addr_t mask = {.addr = netmask_addr};
        ESP_LOGE(TAG, "Invalid netmask: " IPSTR, IP2STR(&mask));
        return ESP_ERR_INVALID_ARG;
    }

    if (!network_ipv4_host_is_usable_for_ap(gw_addr, netmask_addr)) {
        esp_ip4_addr_t ip = {.addr = gw_addr};
        esp_ip4_addr_t mask = {.addr = netmask_addr};

        ESP_LOGE(TAG, "Invalid AP IP/netmask combination, ip=" IPSTR ", netmask=" IPSTR, IP2STR(&ip), IP2STR(&mask));

        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_netif_get_ip_info(wifi_ap_netif, &old_ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_get_ip_info failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Old AP IP=" IPSTR ", GW=" IPSTR ", Netmask=" IPSTR, IP2STR(&old_ip_info.ip), IP2STR(&old_ip_info.gw),
             IP2STR(&old_ip_info.netmask));

    /*
     * DHCP server must be stopped before esp_netif_set_ip_info().
     */
    ret = esp_netif_dhcps_stop(wifi_ap_netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "esp_netif_dhcps_stop failed: %s", esp_err_to_name(ret));
        return ret;
    }

    new_ip_info = old_ip_info;

    /*
     * Keep existing behavior:
     * gw_addr is used as both SoftAP IP and gateway.
     *
     * If you only want to change gateway but keep AP IP unchanged,
     * remove this line:
     *     new_ip_info.ip.addr = gw_addr;
     */
    new_ip_info.ip.addr = gw_addr;
    new_ip_info.gw.addr = gw_addr;
    new_ip_info.netmask.addr = netmask_addr;

    ret = esp_netif_set_ip_info(wifi_ap_netif, &new_ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_set_ip_info failed: %s", esp_err_to_name(ret));

        /*
         * Best effort: restart DHCP server before returning,
         * otherwise AP DHCP may remain stopped.
         */
        start_ret = esp_netif_dhcps_start(wifi_ap_netif);
        if (start_ret != ESP_OK && start_ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            ESP_LOGE(TAG, "Failed to restart DHCP server: %s", esp_err_to_name(start_ret));
        }

        return ret;
    }

    /*
     * Configure DHCP server to optionally offer DNS.
     */
    dhcps_offer_t dhcp_option = old_internet_connection_flag ? OFFER_DNS : OFFER_START;

    ret = esp_netif_dhcps_option(wifi_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcp_option,
                                 sizeof(dhcp_option));
    if (ret != ESP_OK) {
        /*
         * DNS offer failure should not necessarily fail AP IP/netmask update.
         */
        ESP_LOGW(TAG, "Failed to set DHCP DNS option: %s", esp_err_to_name(ret));
    }

    start_ret = esp_netif_dhcps_start(wifi_ap_netif);
    if (start_ret != ESP_OK && start_ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGE(TAG, "esp_netif_dhcps_start failed: %s", esp_err_to_name(start_ret));
        return start_ret;
    }

    ESP_LOGI(TAG, "New AP IP=" IPSTR ", GW=" IPSTR ", Netmask=" IPSTR, IP2STR(&new_ip_info.ip), IP2STR(&new_ip_info.gw),
             IP2STR(&new_ip_info.netmask));

    if (old_internet_connection_flag) {
        if (s_napt_enabled && old_ip_info.ip.addr && old_ip_info.ip.addr != gw_addr) {
            ip_napt_enable(old_ip_info.ip.addr, 0);
            s_napt_enabled = false;
            s_napt_bound_ap_ip = 0;
        }

        network_wifi_ap_refresh_uplink_state();
    }

    net_state.current_ap_ip.gateway = gw_addr;
    net_state.current_ap_ip.netmask = netmask_addr;

    return ESP_OK;
}

void network_wifi_ap_get_name(char *out, size_t len)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, len, NETWORK_DEFAULT_AP_SSID_PREFIX "_%02X%02X", mac[4], mac[5]);
}

/**
 * @brief Update the DNS information for the given data forw arding network interface.
 *
 * This function checks if the provided DNS info differs from the current DNS info
 * of the network interface, and updates it if necessary.
 *
 * @param data_forwarding_netif Pointer to the network interface to update.
 * @param dns_info Pointer to the new DNS info to set.
 */
static void esp_bridge_update_data_forwarding_netif_dns_info(esp_netif_t *data_forwarding_netif,
                                                             esp_netif_dns_info_t *dns_info)
{
    if (!data_forwarding_netif) {
        return;
    }

    esp_netif_dns_info_t old_dns_info = {0};
    esp_netif_get_dns_info(data_forwarding_netif, ESP_NETIF_DNS_MAIN, &old_dns_info);
    if (old_dns_info.ip.u_addr.ip4.addr == dns_info->ip.u_addr.ip4.addr) {
        return;
    }
    ESP_ERROR_CHECK(esp_netif_set_dns_info(data_forwarding_netif, ESP_NETIF_DNS_MAIN, dns_info));
    ESP_LOGI(TAG, "[%-12s]Name Server1: " IPSTR, esp_netif_get_ifkey(data_forwarding_netif),
             IP2STR(&dns_info->ip.u_addr.ip4));
}

/**
 * @brief Update the DNS info of the data forwarding interface from an external interface.
 *
 * If the external interface has no DNS set, it uses a configured static DNS server address.
 * Then it applies this DNS info to the data forwarding interface.
 *
 * @param external_netif Pointer to the external network interface (e.g., STA or ETH).
 * @param data_forwarding_netif Pointer to the data forwarding network interface (e.g., SoftAP).
 *
 * @return ESP_OK on success.
 */
static esp_err_t esp_bridge_update_dns_info(esp_netif_t *external_netif, esp_netif_t *data_forwarding_netif)
{
    esp_netif_dns_info_t dns_info = {0};
    if (external_netif) {
        esp_netif_get_dns_info(external_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    }
    if (dns_info.ip.u_addr.ip4.addr == 0) {
        dns_info.ip.u_addr.ip4.addr = ipaddr_addr(CONFIG_BRIDGE_STATIC_DNS_SERVER_MAIN);
        dns_info.ip.type = IPADDR_TYPE_V4;
    }

    if (data_forwarding_netif) {
        esp_bridge_update_data_forwarding_netif_dns_info(data_forwarding_netif, &dns_info);
    }
    else {
        esp_bridge_update_data_forwarding_netif_dns_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &dns_info);
    }
    return ESP_OK;
}

static esp_netif_t *network_get_active_uplink_netif(void)
{
    if (net_state.sta_connected_flag) {
        return network_sta_get_netif();
    }

    return NULL;
}

/**
 * @brief Check if the Access Point network interface IP is present and active in the netif list.
 *
 * Iterates over all netif interfaces and returns true if any has the same IP and is up.
 *
 * @param ap_ip Pointer to the AP IP info structure to check.
 *
 * @return true if the AP IP is present and active, false otherwise.
 */
static bool is_ap_netif_present(esp_netif_ip_info_t *ap_ip)
{
    struct netif *netif;
    for (netif = netif_list; netif; netif = netif->next) {
        if (netif_is_up(netif) && !ip_addr_isany(&netif->ip_addr) &&
            ip_2_ip4(&netif->ip_addr)->addr == ap_ip->ip.addr) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Configure the DHCP server on the AP interface to either offer DNS or not.
 *
 * Stops the DHCP server, sets the DNS offering option based on the flag, then restarts the DHCP server.
 *
 * @param ap_netif Pointer to the AP network interface.
 * @param flag Set to 1 to enable DNS offering, 0 to disable.
 *
 * @return ESP_OK on success.
 */
static esp_err_t configure_ap_dhcp_server(esp_netif_t *ap_netif, int flag)
{
    dhcps_offer_t dhcp_offer = (flag == 1) ? OFFER_DNS : OFFER_START;

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcp_offer,
                                           sizeof(dhcp_offer)));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    return ESP_OK;
}

/**
 * @brief Refresh AP uplink DNS/NAPT state without restarting SoftAP.
 *
 * Updates AP-side DNS according to the currently active uplink (ETH preferred
 * over STA), restarts the DHCP server so new DNS is offered to stations, and
 * rebinds NAPT to the current AP IP.
 *
 * @return ESP_OK on success or an error code on failure.
 */
esp_err_t network_wifi_ap_refresh_uplink_state(void)
{
    esp_netif_t *softap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_t *uplink_netif = network_get_active_uplink_netif();
    esp_netif_ip_info_t ap_ip_info = {0};
    esp_err_t ret;

    if (!softap_netif) {
        ESP_LOGE(TAG, "SoftAP netif not found");
        return ESP_FAIL;
    }

    if (!old_internet_connection_flag) {
        return ESP_OK;
    }

    ret = esp_netif_get_ip_info(softap_netif, &ap_ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP IP info");
        return ret;
    }

    if (!is_ap_netif_present(&ap_ip_info)) {
        ESP_LOGE(TAG, "AP address not found in netif list while refreshing uplink state");
        return ESP_FAIL;
    }

    if (uplink_netif) {
        ret = esp_bridge_update_dns_info(uplink_netif, softap_netif);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to refresh AP DNS: %s", esp_err_to_name(ret));
        }
    }
    else {
        ESP_LOGW(TAG, "No active uplink netif while refreshing AP state");
    }

    ret = configure_ap_dhcp_server(softap_netif, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to restart AP DHCP server: %s", esp_err_to_name(ret));
        return ret;
    }

    if (s_napt_enabled && s_napt_bound_ap_ip && s_napt_bound_ap_ip != ap_ip_info.ip.addr) {
        ip_napt_enable(s_napt_bound_ap_ip, 0);
        s_napt_enabled = false;
        s_napt_bound_ap_ip = 0;
    }

    if (!s_napt_enabled) {
        ip_napt_enable(ap_ip_info.ip.addr, 1);
        s_napt_enabled = true;
        s_napt_bound_ap_ip = ap_ip_info.ip.addr;
    }

    return ESP_OK;
}

/**
 * @brief Enable or disable internet connection sharing on the Wi-Fi AP interface.
 *
 * This function updates the DHCP server configuration and NAT (NAPT) settings
 * based on the provided flag. It ensures the AP netif is valid and checks for
 * its presence before and after configuration changes.
 *
 * @param[in] flag Set to 1 to enable internet connection sharing, 0 to disable.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL if the SoftAP interface is not found or IP info retrieval fails,
 *        or if AP netif is not present when expected.
 */
esp_err_t network_wifi_ap_internet_connection_enable(int flag)
{
    ESP_LOGW(TAG, "ap start internet_connection flag：%d", flag);
    esp_netif_t *softap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ap_ip_info = {0};

    if (!softap_netif) {
        ESP_LOGE(TAG, "SoftAP netif not found");
        return ESP_FAIL;
    }

    if (flag == 0) {
        if (s_napt_enabled && s_napt_bound_ap_ip) {
            ip_napt_enable(s_napt_bound_ap_ip, 0);
            s_napt_enabled = false;
            s_napt_bound_ap_ip = 0;
        }

        configure_ap_dhcp_server(softap_netif, 0);
        old_internet_connection_flag = 0;
        return ESP_OK;
    }

    if (esp_netif_get_ip_info(softap_netif, &ap_ip_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP IP info");
        return ESP_FAIL;
    }

    if (!is_ap_netif_present(&ap_ip_info)) {
        ESP_LOGE(TAG, "AP address not found in netif list while enabling internet connection");
        return ESP_FAIL;
    }

    old_internet_connection_flag = 1;
    return network_wifi_ap_refresh_uplink_state();
}

/**
 * @brief Get the current status of the Wi-Fi Access Point (AP).
 *
 * @return true if the AP is enabled and accepting connections, false otherwise.
 */
bool network_wifi_get_ap_enabled(void)
{
    return ap_start_flag;
}

static void network_wifi_ap_reset_runtime_forwarding_state(void)
{
    if (s_napt_enabled && s_napt_bound_ap_ip) {
        ip_napt_enable(s_napt_bound_ap_ip, 0);
    }

    s_napt_enabled = false;
    s_napt_bound_ap_ip = 0;
    old_internet_connection_flag = 0;
}

/**
 * @brief Disable the Wi-Fi Access Point connection.
 *
 * This function hides the AP SSID and sets the maximum number of allowed connections to zero,
 * effectively disabling new client connections.
 *
 * @return ESP_OK on success.
 */
esp_err_t network_wifi_ap_disable_connection(void)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_APSTA) {
        network_wifi_ap_reset_runtime_forwarding_state();
        esp_wifi_deauth_sta(0);
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_mode(WIFI_MODE_STA) failed: %s", esp_err_to_name(err));
            return err;
        }
        ap_start_flag = false;
        ESP_LOGI(TAG, "AP fully stopped");
    }
    return ESP_OK;
}

/**
 * @brief Enable the Wi-Fi Access Point connection.
 *
 * This function makes the AP SSID visible and restores the maximum number of allowed connections
 * to the configured limit, enabling client connections.
 *
 * @return ESP_OK on success.
 */
esp_err_t network_wifi_ap_enable_connection(void)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode != WIFI_MODE_APSTA) {
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_mode(WIFI_MODE_APSTA) failed: %s", esp_err_to_name(err));
            return err;
        }

        ap_start_flag = true;
        ESP_LOGI(TAG, "AP fully started");
    }
    return ESP_OK;
}

/**
 * @brief Configure and enable/disable the Wi-Fi AP mode with specified SSID and password.
 *
 * If password is invalid (NULL or less than MIN_PASSWORD_LENGTH), open network is created.
 *
 * @param[in] enable        If false, enables connection; if true, disables connection.
 * @param[in] ssid          Pointer to SSID string (can be NULL to use default).
 * @param[in] password      Pointer to password string (NULL or < MIN_PASSWORD_LENGTH means open AP).
 * @param[in] internet_connection  Not used in this function but may control external logic.
 * @param ssid_hidden
 * @param ap_channel
 *
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t network_wifi_ap_handler_set_mode(bool enable, const uint8_t *ssid, const uint8_t *password,
                                           bool internet_connection, bool ssid_hidden, uint32_t ap_channel)
{
    // Determine if AP should be open based on password length
    bool is_open_ap = (!password || strlen((const char *)password) < MIN_PASSWORD_LENGTH);
    wifi_auth_mode_t auth_mode = is_open_ap ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    // Prepare AP config
    wifi_config_t wifi_config = {.ap = {.max_connection = AP_MAX_CONNECT_NUM,
                                        .authmode = auth_mode,
                                        .channel = ap_channel == 0 ? (esp_random() % 10) + 1 : ap_channel,
                                        .ssid_hidden = ssid_hidden}};

    memcpy((char *)wifi_config.ap.ssid, (const char *)ssid, sizeof(wifi_config.ap.ssid));

    // Set password if secure mode
    if (auth_mode == WIFI_AUTH_WPA2_PSK && password) {
        strlcpy((char *)wifi_config.ap.password, (const char *)password, sizeof(wifi_config.ap.password));
    }

    ESP_LOGW(TAG, "AP SSID: %s, AP PASSWORD: %s, Channel: %d, Auth: %d", wifi_config.ap.ssid, wifi_config.ap.password,
             wifi_config.ap.channel, auth_mode);

    // Handle AP connection mode switch
    if (enable) {
        esp_err_t err = network_wifi_ap_enable_connection();
        if (err != ESP_OK) {
            return err;
        }
        // Apply configuration to AP interface
        err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
        ESP_LOGW(TAG, "esp_wifi_set_config WIFI_IF_AP: %s", esp_err_to_name(err));
        if (err != ESP_OK) {
            return err;
        }

        wifi_config_t active_config = {0};
        esp_err_t get_config_err = esp_wifi_get_config(WIFI_IF_AP, &active_config);
        if (get_config_err == ESP_OK) {
            size_t ssid_len = active_config.ap.ssid_len;
            if (ssid_len == 0 || ssid_len > sizeof(active_config.ap.ssid)) {
                ssid_len = strnlen((const char *)active_config.ap.ssid, sizeof(active_config.ap.ssid));
            }
            ESP_LOGI(TAG, "SoftAP started successfully, actual SSID: %.*s", (int)ssid_len,
                     (const char *)active_config.ap.ssid);
        }
        else {
            ESP_LOGW(TAG, "Failed to read active SoftAP configuration: %s", esp_err_to_name(get_config_err));
        }
        if (internet_connection) {
            ESP_LOGW(TAG, "ap start internet_con");
            vTaskDelay(pdMS_TO_TICKS(500));
            network_wifi_ap_internet_connection_enable(0);
            network_wifi_ap_internet_connection_enable(1);
        }
    }
    else {
        network_wifi_ap_disable_connection();
    }
    return ESP_OK;
}

esp_err_t network_wifi_ap_set_bandwidth(uint8_t band_width)
{
    if (band_width != 20 && band_width != 40) {
        ESP_LOGE(TAG, "Invalid bandwidth value: %d", band_width);
        return ESP_FAIL;
    }
    if (net_state.ap_config.band_width == band_width) {
        return ESP_OK;
    }
    net_state.ap_config.band_width = band_width;
    if (band_width == 40) {
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);
    }
    else {
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    }
    return ESP_OK;
}

/**
 * @brief Initialize the Wi-Fi Access Point (AP) interface.
 *
 * This function creates the default Wi-Fi AP network interface and configures
 * DHCP server options.
 *
 * @return
 *      - ESP_OK on success
 *      - Error code from esp_netif APIs on failure
 */
esp_err_t network_wifi_ap_init(void)
{
    wifi_ap_netif = esp_netif_create_default_wifi_ap();
    dhcps_offer_t dhcps_dns_value = OFFER_START;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(wifi_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &dhcps_dns_value, sizeof(dhcps_dns_value)));
    return ESP_OK;
}
