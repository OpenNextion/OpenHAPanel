#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "mdns.h"
#include "cJSON.h"
#include "mdns_api.h"
#include <network_wifi_sta.h>
#include "network.h"
#include <cjson_helpers.h>
#include "littlefs.h"
#include "webserver_api.h"
#include "webserver_method_api.h"
#include "webserver_config_api.h"

static const char *TAG = "mdns_api";

extern char mdns_current_ip[MDNS_IP_LEN];
extern char mdns_custom_hostname[HOSTNAME_MAX_LEN];
extern mdns_scan_result *mdns_prev_scan_results;
extern int16_t mdns_prev_count;
static char mac_id[8] = {0};

bool mdns_server_get_hostname(char *hostname, size_t len)
{
    if (mdns_custom_hostname[0] != '\0') {
        snprintf(hostname, len, "%s", mdns_custom_hostname);
        return true;
    }
    return false;
}

static uint16_t get_mac_address(void)
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGE(TAG, "read Wi-Fi MAC address failed");
        return 0;
    }
    return ((uint16_t)mac[4] << 8) | mac[5];
}

/**
 * @brief Updates the mDNS TXT records for the HTTP service.
 *
 * This function updates the TXT records associated with the "_http._tcp"
 * mDNS service. The TXT records include:
 * - "name": The product name retrieved from getProductName().
 * - "id"  : The product ID retrieved from getProductId().
 *
 * This allows network clients to see up-to-date metadata about the device
 * when discovering services via mDNS.
 */
void mdns_server_update_server_txt()
{
    device_info_t *device_info = webserver_config_get_device_info();
    mdns_txt_item_t serviceTxtData[] = {
        {"name", device_info->product_name},
        {"id", mac_id},
    };
    mdns_service_txt_set("_http", "_tcp", serviceTxtData, 2);
}

/**
 * @brief Checks for hostname conflicts on the local network using mDNS.
 *
 * This function queries the mDNS service for devices advertising an "_http._tcp"
 * service and compares their hostnames against the provided `hostname`.
 *
 * If a device with the same hostname is found, the function reports a conflict.
 * This is useful for ensuring hostname uniqueness before broadcasting or saving
 * the device's mDNS configuration.
 *
 * @param hostname The hostname to check for conflicts.
 *
 * @return int8_t
 *         MDNS_OK           (0)  - No conflict detected.
 *         MDNS_ERR_CONFLICT (1)  - Conflict detected; hostname already in use.
 *         MDNS_FAIL         (-1) - Query failed or an error occurred.
 *
 * Notes:
 * - Uses `mdns_query_ptr()` to perform the mDNS query.
 * - Frees the result list using `mdns_query_results_free()` after processing.
 * - Requires ESP-IDF's mDNS component.
 */
static int8_t mdns_check_hostname_conflict(const char *hostname)
{
    esp_ip4_addr_t addr = {0};
    esp_err_t err = mdns_query_a(hostname, MDNS_SCAN_TIME_MS, &addr);

    if (err == ESP_OK) {
        ESP_LOGW(TAG, "hostname %s already exists: " IPSTR, hostname, IP2STR(&addr));
        return MDNS_ERR_CONFLICT;
    }

    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mDNS is not running");
        return MDNS_FAIL;
    }
    return MDNS_OK;
}

/**
 * @brief Updates the mDNS hostname based on a custom value.
 *
 * This function checks whether a custom hostname (`mdns_custom_hostname`) is set.
 * - If a custom hostname is present (non-empty), it sets it as the mDNS hostname.
 * - Otherwise, it falls back to using the default hostname (`DEFAULTS_HOST_NAME`).
 *
 * The updated hostname will be broadcast via mDNS, allowing other devices
 * on the local network to resolve the device using the new name.
 *
 * Logs the selected hostname using ESP_LOGI for debugging purposes.
 */
void mdns_update_server_by_host_name(bool conflict_avoid_flag)
{
    ESP_LOGI(TAG, "update broadcast mdns server");
    char temp[128] = {0};
    bool use_default_hostname = mdns_custom_hostname[0] == '\0' ||
                                strcmp(mdns_custom_hostname, DEFAULTS_HOST_NAME) == 0;
    if (!use_default_hostname) {
        snprintf(temp, sizeof(temp), "%s", mdns_custom_hostname);
        if (conflict_avoid_flag) {
            for (uint16_t i = 0; i < 65535; i++) {
                if (mdns_check_hostname_conflict(temp) == MDNS_ERR_CONFLICT) {
                    ESP_LOGW(TAG, "hostname conflict, hostname: %s", temp);
                    snprintf(temp, sizeof(temp), "%s-%d", mdns_custom_hostname, i + 2);
                    temp[63] = '\0';
                    continue;
                }
                break;
            }
        }
    }
    else {
        snprintf(temp, sizeof(temp), "%s", DEFAULTS_HOST_NAME);
        if (conflict_avoid_flag) {
            int8_t conflict_status = mdns_check_hostname_conflict(temp);
            char active_hostname[HOSTNAME_MAX_LEN] = {0};
            mdns_hostname_get(active_hostname);

            /* ESP-IDF's mDNS responder can resolve a collision on its own by
             * changing "nextion" to "nextion-2" while the A query above is
             * still running. Treat that automatic rename as proof of a
             * conflict and replace it with our stable MAC-based hostname. */
            bool responder_renamed = active_hostname[0] != '\0' &&
                                     strcmp(active_hostname, DEFAULTS_HOST_NAME) != 0;
            if (conflict_status == MDNS_ERR_CONFLICT || responder_renamed) {
                uint8_t esp_mac[6] = {0};
                esp_read_mac(esp_mac, ESP_MAC_WIFI_STA);
                snprintf(temp, sizeof(temp), DEFAULTS_HOST_NAME "-%02x%02x", esp_mac[4], esp_mac[5]);
                ESP_LOGW(TAG, "default hostname conflict; using %s", temp);
            }
        }
    }
    esp_err_t err = mdns_hostname_set(temp);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "mDNS hostname applied successfully: %s", temp);
    }
    else {
        ESP_LOGE(TAG, "Failed to apply mDNS hostname '%s': %s", temp, esp_err_to_name(err));
    }
}

/**
 * @brief Saves the current mDNS hostname configuration to a file in JSON format.
 *
 * This function checks whether the custom mDNS hostname is set (non-empty).
 * If valid, it serializes the hostname into a JSON object and writes it to
 * a persistent configuration file (defined by MDNS_CONFIG_FILE_NAME).
 *
 * The saved configuration can later be used to restore the hostname after
 * a reboot or system reset.
 *
 * Error handling:
 * - Returns -1 if the hostname is empty.
 * - Returns -1 if JSON creation or serialization fails.
 * - Returns -1 if writing to the file fails.
 * - Returns 0 on success.
 *
 * @return int8_t
 *         0  - Success
 *        -1  - Failure (with error log output)
 *
 * Dependencies:
 * - Assumes `mdns_custom_hostname` is a valid global variable.
 * - Uses `cJSON` for JSON handling.
 * - Uses `little_file_system_write_string_to_file()` for file writing.
 */
static int8_t mdns_save_config(void)
{
    int8_t result = ESP_FAIL;
    if (strlen(mdns_custom_hostname) == 0) {
        ESP_LOGE(TAG, "Cannot save config: hostname is empty");
        return result;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root object");
        return result;
    }
    cJSON_AddStringToObject(root, "hostname", mdns_custom_hostname);
    if (!little_file_system_write_json_to_file(root, MDNS_CONFIG_FILE_NAME)) {
        ESP_LOGE(TAG, "Failed to write mdns config to file");
        return result;
    }
    return ESP_OK;
}

/**
 * @brief Sets a new mDNS hostname after validating it and checking for conflicts.
 *
 * This function attempts to set a new mDNS hostname by:
 * 1. Verifying that the hostname does not exceed the maximum allowed length (`HOSTNAME_MAX_LEN`).
 * 2. Checking the local network (via mDNS) to ensure the hostname is not already in use.
 * 3. If the hostname is valid and not in conflict, it updates the internal `mdns_custom_hostname`
 *    and triggers the mDNS server to update its hostname broadcast.
 *
 * If a conflict is detected, this function triggers internal notifications:
 * - `notification_manage_mdns_conflicts_notify_tigger()`
 * - `webhook_manage_task_event_tigger()` with `TIGGER_TYPE_MDNS_CONFLICTS`
 *
 * @param hostname The desired hostname to be set for mDNS broadcast.
 *
 * @return int16_t
 *         MDNS_OK                (0)   - Hostname set successfully.
 *         MDNS_ERR_PARAM         (-1)  - Hostname length exceeds allowed maximum.
 *         MDNS_ERR_DOMAIN_CONFLICT (-2) - Hostname is already in use on the network.
 *
 * Notes:
 * - Relies on `mdns_check_hostname_conflict()` to detect duplicates.
 * - Assumes `mdns_custom_hostname` is a globally defined buffer.
 */
static int16_t mdns_set_mdns_hostname(const char *hostname)
{
    if (strlen(hostname) >= HOSTNAME_MAX_LEN) {
        return MDNS_ERR_PARAM;
    }

    if (mdns_check_hostname_conflict(hostname) < 0) {
        ESP_LOGE(TAG, "set hostname conflict\n");
        // notification_manage_mdns_conflicts_notify_tigger();
        // webhook_manage_task_event_tigger(TIGGER_TYPE_MDNS_CONFLICTS);
        return MDNS_ERR_DOMAIN_CONFLICT;
    }

    ESP_LOGI(TAG, "SET hostname %s", hostname);
    strncpy(mdns_custom_hostname, hostname, sizeof(mdns_custom_hostname));
    mdns_update_server_by_host_name(false);
    return MDNS_OK;
}

/**
 * @brief Handles a Web API request to set the mDNS domain name.
 *
 * This function parses a JSON payload from the incoming request and extracts
 * the `"mdnsDomain"` field. It validates the format of the domain, ensuring it
 * ends with the required suffix (`MDNS_DST`), then strips the suffix to get the
 * hostname portion. The hostname is then set via `mdns_set_mdns_hostname()`.
 *
 * If the operation is successful, the hostname is persisted to config,
 * MQTT URLs are synchronized, and the configuration URL is updated.
 *
 * A response is sent back to the client via `webserver_code_response_to_current_client()`,
 * including the original request sequence number.
 *
 * @param data           JSON object containing the "mdnsDomain" field.
 * @param now_resp_arg   Pointer to async response argument (contains sequence ID).
 *
 * @return esp_err_t
 *         ESP_OK   - on success
 *         ESP_FAIL - on failure or validation error
 *
 * Expected JSON input format:
 * {
 *     "mdnsDomain": "my-device.local"
 * }
 */
esp_err_t method_api_handle_set_mdns_domain(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t result = ESP_FAIL;

    cJSON *domain_json = cJSON_GetObjectItem(data, "mdnsDomain");
    if (!cJSON_IsString(domain_json)) {
        ESP_LOGE(TAG, "Missing or invalid 'mdnsDomain' string");
        goto response;
    }

    const char *full_domain = domain_json->valuestring;
    size_t full_len = strlen(full_domain);
    size_t suffix_len = strlen(MDNS_DST);

    if (full_len <= suffix_len || strcmp(full_domain + full_len - suffix_len, MDNS_DST) != 0) {
        ESP_LOGE(TAG, "Invalid mdnsDomain format: must end with %s", MDNS_DST);
        goto response;
    }

    size_t hostname_len = full_len - suffix_len;
    if (hostname_len > 30) {
        ESP_LOGE(TAG, "Hostname part is too long (max %d bytes)", 30);
        result = WEB_API_ERROR_PARAM_LEN;
        goto response;
    }

    char hostname[HOSTNAME_MAX_LEN] = {0};
    snprintf(hostname, sizeof(hostname), "%.*s", (int)hostname_len, full_domain);

    result = mdns_set_mdns_hostname(hostname);
    if (result == MDNS_OK) {
        network_wifi_sta_update_hostname();
        // config_set_configuration_url(hostname);
        // mqtt_sync_url();
        mdns_save_config();
        webserver_code_response_to_current_client(result, WEB_API_METHOD_NET_SET_MDNS_DOMAIN_RSP, now_resp_arg->seq);
        esp32_app_restart();
        return result;
    }
    else {
        ESP_LOGE(TAG, "Failed to set mDNS hostname");
    }

response:
    webserver_code_response_to_current_client(result, WEB_API_METHOD_NET_SET_MDNS_DOMAIN_RSP, now_resp_arg->seq);
    return result;
}

/**
 * @brief Creates a cJSON object representing a single mDNS device.
 *
 * Constructs a JSON object with device information including:
 * - "id"        : Device identifier string.
 * - "name"      : Device display name.
 * - "mdnsDomain": Fully qualified mDNS domain name (hostname + MDNS_DST suffix).
 * - "ip"        : IP address string of the device.
 *
 * @param id        Unique identifier of the device.
 * @param name      Human-readable device name.
 * @param hostname  Hostname part of the device's mDNS domain.
 * @param ip        IP address of the device.
 *
 * @return cJSON*   Pointer to the newly created cJSON object representing the device.
 */
static cJSON *create_mdns_device_item(const char *id, const char *name, const char *hostname, const char *ip)
{
    char domain[75] = {0};
    snprintf(domain, sizeof(domain), "%s%s", hostname, MDNS_DST);

    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "id", id);
    cJSON_AddStringToObject(device, "name", name);
    cJSON_AddStringToObject(device, "mdnsDomain", domain);
    cJSON_AddStringToObject(device, "ip", ip);
    return device;
}

/**
 * @brief Notifies the webserver with the current cached mDNS scan results.
 *
 * This function prepares a JSON payload containing the local device information
 * as well as previously scanned mDNS devices stored in `mdns_prev_scan_results`.
 * It sends the data to the web interface using `webserver_data_response_to_all_client()` with the
 * event identifier "Device.LanDevicesNotify".
 *
 * If no previous scan results are available (`mdns_prev_scan_results` is NULL and
 * `mdns_prev_count` is zero), a warning is logged and only the local device information
 * is sent.
 *
 * The JSON structure is as follows:
 * {
 *     "devices": [ { "id": ..., "name": ..., "mdnsDomain": ..., "ip": ... }, ... ]
 * }
 *
 * Uses the helper function `create_mdns_device_item()` to generate each device's JSON object.
 */
void webserver_cur_mdns_scan_result_notify(void)
{
    if (!mdns_prev_scan_results && mdns_prev_count == 0) {
        ESP_LOGW(TAG, "No scan results to upload");
    }

    cJSON *data = cJSON_CreateObject();
    cJSON *devices = cJSON_CreateArray();

    /* add this device information */
    char hostname[MDNS_NAME_BUF_LEN] = {0};
    mdns_hostname_get(hostname);
    device_info_t *device_info = webserver_config_get_device_info();
    snprintf(mac_id, sizeof(mac_id), "%04X", (unsigned int)get_mac_address());

    char self_ip[MDNS_IP_LEN] = {0};
    NetworkState *net = network_get_state();
    if (net && !net->sta_connected_flag && !net->eth_connected && net->ap_config.active) {
        esp_ip4_addr_t ap_addr;
        ap_addr.addr = net->current_ap_ip.gateway;
        sprintf(self_ip, IPSTR, IP2STR(&ap_addr));
        ESP_LOGI(TAG, "Provisioning phase, using AP IP: %s", self_ip);
    } else {
        strncpy(self_ip, mdns_current_ip, MDNS_IP_LEN - 1);
    }

    cJSON *self_device = create_mdns_device_item(mac_id, device_info->product_name, hostname,
                                                 self_ip);
    cJSON_AddItemToArray(devices, self_device);

    /* add the scanned device information */
    for (int i = 0; i < mdns_prev_count; ++i) {
        cJSON *device =
            create_mdns_device_item(mdns_prev_scan_results[i].txt_id, mdns_prev_scan_results[i].txt_username,
                                    mdns_prev_scan_results[i].hostname, mdns_prev_scan_results[i].ip_addr);
        cJSON_AddItemToArray(devices, device);
    }

    cJSON_AddItemToObject(data, "devices", devices);
    webserver_data_response_to_all_client(data, WEB_API_METHOD_MDNS_NOTIFY, 0);
}

/**
 * @brief Load mDNS configuration from a JSON file.
 *
 * This function reads the mDNS configuration from a predefined file,
 * parses the JSON content, and extracts the hostname value.
 * If the file is missing, empty, or contains invalid JSON, it falls back
 * to updating the mDNS server with the default or current hostname.
 *
 * @return int8_t
 *         0  - Configuration loaded successfully or file is empty/not found (uses default hostname).
 *         -1 - Failed to parse JSON or invalid hostname in the config file.
 */
int8_t mdns_load_config_from_file(void)
{
    cJSON *root = little_file_system_read_file_to_json(MDNS_CONFIG_FILE_NAME);
    if (root == NULL) {
        return -1;
    }

    cJSON *hostname_json = cJSON_GetObjectItemCaseSensitive(root, "hostname");
    if (!cJSON_IsString(hostname_json) || hostname_json->valuestring[0] == '\0') {
        ESP_LOGE(TAG, "Invalid or missing 'hostname' in config");
        cJSON_Delete(root);
        return -1;
    }
    strncpy(mdns_custom_hostname, hostname_json->valuestring, HOSTNAME_MAX_LEN - 1);
    mdns_custom_hostname[HOSTNAME_MAX_LEN - 1] = '\0';
    cJSON_Delete(root);
    memset(mdns_current_ip, 0, sizeof(mdns_current_ip));
    ESP_LOGW(TAG, "mdns_load_config_from_file mdns : %s", mdns_custom_hostname);
    return 0;
}
