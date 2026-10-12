#include <string.h>
#include "esp_mac.h"
#include "esp_log.h"
#include <esp_wifi.h>
#include <cJSON.h>
#include "littlefs.h"
#include "network_file.h"
#include "network_wifi_sta.h"
#include "network_wifi_ap.h"
#include "cjson_helpers.h"

static const char *TAG = "network_file";

/**
 * @brief Filename for storing network configuration data in JSON format.
 */
#define NETWORK_INFO_FILE_NAME "network_info.json"

static bool network_static_ip_config_is_valid(const network_ip_config_t *config)
{
    return config && config->ip != 0 && config->netmask != 0 &&
           config->gateway != 0 && config->dns1 != 0;
}

/**
 * @brief Log an IPv4 address in human-readable format.
 *
 * Converts a 32-bit IPv4 address to a string representation and
 * logs it with the tag defined by TAG.
 *
 * @param target_ip The IPv4 address in network byte order to print.
 */
static void print_ip(char *ip_name, uint32_t target_ip)
{
    ip4_addr_t ip = {.addr = target_ip};
    ESP_LOGI(TAG, "print %s : %s", ip_name, ip4addr_ntoa(&ip));
}

/**
 * @brief Save the given network configuration to a JSON file.
 *
 * Converts the provided NetworkState structure into a JSON object,
 * including IP settings, Wi-Fi parameters, AP parameters, and modes,
 * and writes it to persistent storage.
 *
 * @param network_config The network configuration structure to save.
 *
 * @return
 * - ESP_OK on successful write to file.
 * - ESP_FAIL if JSON creation or file write fails.
 */
int network_save_config_to_file(NetworkState network_config)
{
    esp_err_t ret = ESP_FAIL;
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "network_save_config_to_file JSON failed");
        return ret;
    }

    cJSON_AddNumberToObject(root, "eth_ip_mode", network_config.eth_ip_mode);
    cJSON_AddNumberToObject(root, "wlan_ip_mode", network_config.sta_ip_mode);

    cJSON_AddNumberToObject(root, "wlan_static_ip.ip", (double)network_config.sta_static_ip.ip);
    cJSON_AddNumberToObject(root, "wlan_static_ip.netmask", (double)network_config.sta_static_ip.netmask);
    cJSON_AddNumberToObject(root, "wlan_static_ip.gateway", (double)network_config.sta_static_ip.gateway);
    cJSON_AddNumberToObject(root, "wlan_static_ip.dns1", (double)network_config.sta_static_ip.dns1);
    cJSON_AddNumberToObject(root, "wlan_static_ip.dns2", (double)network_config.sta_static_ip.dns2);

    cJSON_AddNumberToObject(root, "eth_static_ip.ip", (double)network_config.eth_static_ip.ip);
    cJSON_AddNumberToObject(root, "eth_static_ip.netmask", (double)network_config.eth_static_ip.netmask);
    cJSON_AddNumberToObject(root, "eth_static_ip.gateway", (double)network_config.eth_static_ip.gateway);
    cJSON_AddNumberToObject(root, "eth_static_ip.dns1", (double)network_config.eth_static_ip.dns1);
    cJSON_AddNumberToObject(root, "eth_static_ip.dns2", (double)network_config.eth_static_ip.dns2);

    cJSON_AddStringToObject(root, "wifi_params.ssid", (char *)network_config.wifi_params.ssid);
    cJSON_AddStringToObject(root, "wifi_params.password", (char *)network_config.wifi_params.password);
    cJSON_AddStringToObject(root, "ap_params.ssid", (char *)network_config.ap_params.ssid);
    cJSON_AddStringToObject(root, "ap_params.password", (char *)network_config.ap_params.password);

    cJSON_AddBoolToObject(root, "ap_active", network_config.ap_config.active);
    cJSON_AddBoolToObject(root, "ap_default_active", network_config.ap_config.default_active);
    cJSON_AddBoolToObject(root, "ap_enable_internet_connect", network_config.ap_config.internet_connect);
    cJSON_AddBoolToObject(root, "ap_enable_hide_ssid", network_config.ap_config.hide_ssid);

    cJSON_AddNumberToObject(root, "ap_params.gateway", (double)network_config.current_ap_ip.gateway);
    cJSON_AddNumberToObject(root, "ap_params.netmask", (double)network_config.current_ap_ip.netmask);
    cJSON_AddNumberToObject(root, "ap_params.channel", network_config.ap_config.channel);
    cJSON_AddNumberToObject(root, "ap_params.band_width", network_config.ap_config.band_width);

    if (little_file_system_write_json_to_file(root, NETWORK_INFO_FILE_NAME)) {
        ret = ESP_OK;
    }
    print_ip("sta_static_ip", network_config.sta_static_ip.ip);
    print_ip("eth_static_ip", network_config.eth_static_ip.ip);
    return ret;
}

/**
 * @brief Load network configuration from a JSON file into network state.
 *
 * Reads the network configuration JSON from persistent storage (file system),
 * parses various IP mode settings, IP addresses, Wi-Fi parameters, and AP parameters,
 * and populates the global `net_state` structure accordingly.
 *
 * @note This function uses the cJSON library for JSON parsing and expects
 *       the configuration file to exist and be well-formed.
 *
 * @return
 * - ESP_OK (0) if the configuration was successfully loaded and parsed.
 * - ESP_FAIL if the configuration file could not be read or parsed.
 */
static int network_load_config_from_file(void)
{
    cJSON *root = little_file_system_read_file_to_json(NETWORK_INFO_FILE_NAME);
    if (!root) {
        return ESP_FAIL;
    }
    cjson_parse_int_field(root, "eth_ip_mode", (int *)&net_state.eth_ip_mode);
    cjson_parse_int_field(root, "wlan_ip_mode", (int *)&net_state.sta_ip_mode);
    if (net_state.eth_ip_mode != IP_MODE_STATIC) {
        net_state.eth_ip_mode = IP_MODE_DHCP_AUTO;
    }
    if (net_state.sta_ip_mode != IP_MODE_STATIC) {
        net_state.sta_ip_mode = IP_MODE_DHCP_AUTO;
    }
    cjson_parse_bool_field(root, "ap_active", &net_state.ap_config.active);
    cJSON *ap_default_active = cJSON_GetObjectItemCaseSensitive(root, "ap_default_active");
    bool has_ap_default_active = cJSON_IsBool(ap_default_active);
    if (has_ap_default_active) {
        net_state.ap_config.default_active = cJSON_IsTrue(ap_default_active);
    }
    cjson_parse_bool_field(root, "ap_enable_internet_connect", &net_state.ap_config.internet_connect);
    cjson_parse_bool_field(root, "ap_enable_hide_ssid", &net_state.ap_config.hide_ssid);
    cjson_parse_int_field(root, "ap_params.channel", (int *)&net_state.ap_config.channel);
    cjson_parse_int_field(root, "ap_params.band_width", (int *)&net_state.ap_config.band_width);

    cjson_parse_uint32_field(root, "wlan_static_ip.ip", &net_state.sta_static_ip.ip);
    cjson_parse_uint32_field(root, "wlan_static_ip.netmask", &net_state.sta_static_ip.netmask);
    cjson_parse_uint32_field(root, "wlan_static_ip.gateway", &net_state.sta_static_ip.gateway);
    cjson_parse_uint32_field(root, "wlan_static_ip.dns1", &net_state.sta_static_ip.dns1);
    cjson_parse_uint32_field(root, "wlan_static_ip.dns2", &net_state.sta_static_ip.dns2);

    cjson_parse_uint32_field(root, "eth_static_ip.ip", &net_state.eth_static_ip.ip);
    cjson_parse_uint32_field(root, "eth_static_ip.netmask", &net_state.eth_static_ip.netmask);
    cjson_parse_uint32_field(root, "eth_static_ip.gateway", &net_state.eth_static_ip.gateway);
    cjson_parse_uint32_field(root, "eth_static_ip.dns1", &net_state.eth_static_ip.dns1);
    cjson_parse_uint32_field(root, "eth_static_ip.dns2", &net_state.eth_static_ip.dns2);

    if (net_state.sta_ip_mode == IP_MODE_STATIC &&
        !network_static_ip_config_is_valid(&net_state.sta_static_ip)) {
        ESP_LOGW(TAG, "Invalid STA static IP configuration; falling back to DHCP");
        net_state.sta_ip_mode = IP_MODE_DHCP_AUTO;
    }
    if (net_state.eth_ip_mode == IP_MODE_STATIC &&
        !network_static_ip_config_is_valid(&net_state.eth_static_ip)) {
        ESP_LOGW(TAG, "Invalid Ethernet static IP configuration; falling back to DHCP");
        net_state.eth_ip_mode = IP_MODE_DHCP_AUTO;
    }

    cjson_parse_uint32_field(root, "ap_params.netmask", &net_state.current_ap_ip.netmask);
    cjson_parse_uint32_field(root, "ap_params.gateway", &net_state.current_ap_ip.gateway);

    print_ip("sta_static_ip", net_state.sta_static_ip.ip);
    print_ip("eth_static_ip", net_state.eth_static_ip.ip);

    char *ssid = NULL;
    char *password = NULL;

    if (cjson_parse_string_field(root, "wifi_params.ssid", &ssid)) {
        strlcpy((char *)net_state.wifi_params.ssid, ssid, sizeof(net_state.wifi_params.ssid));
    }

    if (cjson_parse_string_field(root, "wifi_params.password", &password)) {
        strlcpy((char *)net_state.wifi_params.password, password, sizeof(net_state.wifi_params.password));
    }

    /* Legacy configuration only stored the runtime AP state. Treat an active
     * AP as a user default only when STA credentials already existed; an
     * active AP without STA credentials was the automatic provisioning AP. */
    if (!has_ap_default_active) {
        net_state.ap_config.default_active =
            net_state.wifi_params.ssid[0] != '\0' && net_state.ap_config.active;
    }

    char *ap_ssid = NULL;
    if (cjson_parse_string_field(root, "ap_params.ssid", &ap_ssid) && ap_ssid[0] != 0) {
        strlcpy((char *)net_state.ap_params.ssid, ap_ssid, sizeof(net_state.ap_params.ssid));
        ESP_LOGW(TAG, "load ap_params.ssid : %s", (char *)net_state.ap_params.ssid);
    }
    else {
        network_wifi_ap_get_name((char *)net_state.ap_params.ssid, sizeof(net_state.ap_params.ssid));
        ESP_LOGW(TAG, "load ap_params.ssid failed, default ssid : %s", (char *)net_state.ap_params.ssid);
    }

    char *ap_password = NULL;
    if (cjson_parse_string_field(root, "ap_params.password", &ap_password)) {
        strlcpy((char *)net_state.ap_params.password, ap_password, sizeof(net_state.ap_params.password));
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "network load success");

    return ESP_OK;
}

/**
 * @brief Load network configuration from file and apply settings.
 *
 * Attempts to load network configuration from persistent storage.
 * If loading fails, initializes default IP modes and activates AP mode,
 * then saves this default configuration. Sets the AP gateway IP address.
 *
 * Depending on the network state, either initiates Wi-Fi station connection
 * or restarts Ethernet DHCP. Configures Wi-Fi AP mode and internet connection
 * sharing if enabled. Finally, synchronizes the MQTT network status.
 */
void network_load_and_apply_config(void)
{
    int ret = network_load_config_from_file();
    if (ret != 0) {
        ESP_LOGI(TAG, "network load failed, init default");
        net_state.eth_ip_mode = IP_MODE_DHCP_AUTO;
        net_state.sta_ip_mode = IP_MODE_DHCP_AUTO;
        net_state.ap_config.active = true;
        net_state.ap_config.default_active = false;
        net_state.ap_config.channel = 0;
        net_state.ap_config.internet_connect = false;
        net_state.ap_config.hide_ssid = false;
        net_state.ap_config.band_width = 20;
        network_wifi_ap_set_gateway_netmask(ipaddr_addr(NETWORK_DEFAULT_AP_IP), ipaddr_addr("255.255.255.0"));
        network_wifi_ap_get_name((char *)net_state.ap_params.ssid, sizeof(net_state.ap_params.ssid));
        network_save_config_to_file(net_state);
    }
    network_wifi_ap_set_gateway_netmask(net_state.current_ap_ip.gateway, net_state.current_ap_ip.netmask);

    if (net_state.wifi_params.ssid[0] != 0) {
        ESP_LOGI(TAG, "device start, connect wifi");
        if (net_state.sta_ip_mode == IP_MODE_STATIC &&
            net_state.sta_static_ip.ip != 0) {
            network_wifi_sta_stop_dhcp();
        }
        else {
            net_state.sta_ip_mode = IP_MODE_DHCP_AUTO;
            network_wifi_sta_restart_dhcp();
        }
        network_wifi_sta_connect(&net_state.wifi_params);
    }
    else if (net_state.wifi_params.ssid[0] == 0) {
        ESP_LOGI(TAG, "disconnect wifi");
        network_wifi_sta_stop_dhcp();
        network_wifi_sta_disconnect();
    }
    if (net_state.ap_config.band_width == 40) {
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);
    }
    else {
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    }

    /*
     * Apply the user's default AP state on restart:
     * - user default enabled: keep AP on regardless of STA configuration
     * - no Wi-Fi configured: open a temporary provisioning AP
     * - otherwise: keep AP off unless STA failure triggers the fallback AP
     */
    if (net_state.ap_config.default_active) {
        ESP_LOGI(TAG, "AP default is enabled, keep AP active");
        network_wifi_ap_handler_set_mode(true, net_state.ap_params.ssid, net_state.ap_params.password,
                                         net_state.ap_config.internet_connect, net_state.ap_config.hide_ssid,
                                         net_state.ap_config.channel);
        net_state.ap_config.active = true;
    }
    else if (net_state.wifi_params.ssid[0] != 0) {
        ESP_LOGI(TAG, "wifi configured, keep AP off until sta connect fails");
        network_wifi_ap_disable_connection();
        net_state.ap_config.active = false;
    }
    else {
        ESP_LOGI(TAG, "no wifi configured, start provisioning AP");
        network_wifi_ap_handler_set_mode(true, net_state.ap_params.ssid, net_state.ap_params.password,
                                         net_state.ap_config.internet_connect, net_state.ap_config.hide_ssid,
                                         net_state.ap_config.channel);
        net_state.ap_config.active = true;
    }

    // mqtt_sync_network_status(&net_state);
}
