#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "mdns.h"
#include "cJSON.h"
#include "mdns_api.h"
#include <network_wifi_sta.h>
#include "network.h"
#include "network_api.h"
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
/* Legacy mdns.conf with hostname "nextion" denotes the automatic default. */
static bool s_mdns_hostname_explicit = false;
static volatile bool s_mdns_desired_hostname_active = false;

bool mdns_server_get_hostname(char *hostname, size_t len)
{
    if (s_mdns_hostname_explicit && mdns_custom_hostname[0] != '\0') {
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
    uint32_t start_ms = esp_log_timestamp();
    esp_err_t err = mdns_query_a(hostname, MDNS_SCAN_TIME_MS, &addr);
    ESP_LOGI(TAG, "mDNS A conflict query: hostname=%s result=%s elapsed=%" PRIu32 " ms",
             hostname, esp_err_to_name(err), esp_log_timestamp() - start_ms);

    if (err == ESP_OK) {
        ESP_LOGW(TAG, "hostname %s already exists: " IPSTR, hostname, IP2STR(&addr));
        return MDNS_ERR_CONFLICT;
    }

    if (err == ESP_ERR_NOT_FOUND) {
        return MDNS_OK;
    }
    ESP_LOGE(TAG, "mDNS hostname query failed: %s", esp_err_to_name(err));
    return MDNS_FAIL;
}

/* Select the saved hostname, or nextion for factory defaults, after obtaining an IP.
 * A detected conflict keeps the MAC-derived hostname active. */
static esp_err_t mdns_fallback_hostname(char *hostname, size_t len)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err == ESP_OK) {
        snprintf(hostname, len, DEFAULTS_HOST_NAME "-%02x%02x", mac[4], mac[5]);
    }
    return err;
}

esp_err_t mdns_apply_fallback_hostname(void)
{
    char fallback[HOSTNAME_MAX_LEN] = {0};
    esp_err_t err = mdns_fallback_hostname(fallback, sizeof(fallback));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot build fallback mDNS hostname: %s", esp_err_to_name(err));
        return err;
    }

    char active[HOSTNAME_MAX_LEN] = {0};
    if (mdns_hostname_get(active) == ESP_OK && strcmp(active, fallback) == 0) {
        s_mdns_desired_hostname_active = false;
        return ESP_OK;
    }

    err = mdns_hostname_set(fallback);
    if (err == ESP_OK) {
        s_mdns_desired_hostname_active = false;
        ESP_LOGI(TAG, "mDNS fallback hostname applied: %s.local", fallback);
    } else {
        ESP_LOGE(TAG, "Failed to apply mDNS fallback '%s': %s", fallback, esp_err_to_name(err));
    }
    return err;
}

void mdns_select_hostname_for_network(void)
{
    s_mdns_desired_hostname_active = false;
    const char *desired = s_mdns_hostname_explicit ? mdns_custom_hostname : DEFAULTS_HOST_NAME;
    int8_t status = mdns_check_hostname_conflict(desired);
    if (status != MDNS_OK) {
        ESP_LOGW(TAG, "mDNS hostname %s unavailable (%d); keeping MAC fallback", desired, status);
        mdns_apply_fallback_hostname();
        return;
    }

    esp_err_t err = mdns_hostname_set(desired);
    if (err == ESP_OK) {
        s_mdns_desired_hostname_active = true;
        ESP_LOGI(TAG, "mDNS desired hostname applied: %s.local", desired);
    } else {
        ESP_LOGE(TAG, "Failed to apply mDNS hostname '%s': %s", desired, esp_err_to_name(err));
        mdns_apply_fallback_hostname();
    }
}

void mdns_verify_hostname(void)
{
    if (!s_mdns_desired_hostname_active) {
        return;
    }

    char active[HOSTNAME_MAX_LEN] = {0};
    if (mdns_hostname_get(active) != ESP_OK) {
        return;
    }
    const char *desired = s_mdns_hostname_explicit ? mdns_custom_hostname : DEFAULTS_HOST_NAME;
    if (strcmp(active, desired) != 0) {
        ESP_LOGW(TAG, "mDNS responder renamed %s.local to %s.local; using MAC fallback",
                 desired, active);
        if (mdns_apply_fallback_hostname() == ESP_OK) {
            webserver_network_info_notify();
        }
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
    cJSON_AddBoolToObject(root, "custom", s_mdns_hostname_explicit);
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

    int8_t conflict = mdns_check_hostname_conflict(hostname);
    if (conflict == MDNS_ERR_CONFLICT) {
        ESP_LOGE(TAG, "set hostname conflict: %s", hostname);
        return MDNS_ERR_DOMAIN_CONFLICT;
    }
    if (conflict != MDNS_OK) {
        return MDNS_FAIL;
    }

    bool previous_desired_active = s_mdns_desired_hostname_active;
    s_mdns_desired_hostname_active = false;
    esp_err_t err = mdns_hostname_set(hostname);
    if (err != ESP_OK) {
        s_mdns_desired_hostname_active = previous_desired_active;
        ESP_LOGE(TAG, "Failed to set hostname %s: %s", hostname, esp_err_to_name(err));
        return MDNS_FAIL;
    }
    ESP_LOGI(TAG, "SET hostname %s", hostname);
    strncpy(mdns_custom_hostname, hostname, sizeof(mdns_custom_hostname));
    s_mdns_hostname_explicit = true;
    s_mdns_desired_hostname_active = true;
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
        webserver_network_info_notify();
        webserver_code_response_to_current_client(result, WEB_API_METHOD_NET_SET_MDNS_DOMAIN_RSP, now_resp_arg->seq);
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
    cJSON *custom_json = cJSON_GetObjectItemCaseSensitive(root, "custom");
    s_mdns_hostname_explicit = cJSON_IsTrue(custom_json) ||
                               (custom_json == NULL && strcmp(mdns_custom_hostname, DEFAULTS_HOST_NAME) != 0);
    cJSON_Delete(root);
    memset(mdns_current_ip, 0, sizeof(mdns_current_ip));
    ESP_LOGW(TAG, "mdns_load_config_from_file mdns : %s", mdns_custom_hostname);
    return 0;
}
