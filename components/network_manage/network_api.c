#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "lwip/ip_addr.h"
#include "network_api.h"
#include "network.h"
#include "network_file.h"
#include "network_wifi_ap.h"
#include "webserver_config.h"
#include "cjson_helpers.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "webserver_auth.h"
#include "provisioning_handoff.h"
#include "network_wifi_sta.h"
#include "mdns_api.h"

static const char *TAG = "network_info";

#define NETWORK_SCAN_DURATION "duration"
#define NETWORK_IP_ACQUISITION "ipAcquisition"
#define NETWORK_IP_ADDRESS "ipAddress"
#define NETWORK_SUBNET_MASK "subnetMask"
#define NETWORK_DNS1 "dns1"
#define NETWORK_GATEWAY "gateway"
#define NETWORK_DNS2 "dns2"
#define NETWORK_WIFI_IV "iv"
#define NETWORK_WIFI_CONTENT "content"
#define NETWORK_MODE "networkMode"
#define INTERNET_CON "internetConnected"
#define NETWORK_ETHERNET "ethernet"
#define NETWORK_WIFI "wifi"
#define NETWORK_WIFI_SSID "ssid"
#define NETWORK_WIFI_PASSWORD "password"
#define NETWORK_WIFI_BSSID "bssid"
#define NETWORK_AP "ap"
#define NETWORK_AP_INTERNET "internetThroughAp"
#define NETWORK_AP_ENABLED "enabled"
#define NETWORK_AP_DEFAULT_ENABLED "defaultEnabled"
#define NETWORK_AP_SSID "ssid"
#define NETWORK_AP_HIDE_SSID "hideSsid"
#define NETWORK_AP_CHANNEL "channel"
#define NETWORK_AP_BANDWIDTH "bandwidth"
#define NETWORK_AP_GATEWAY "gateway"
#define NETWORK_AP_SUBNET_MASK "subnetMask"

#define HASS_MDNS_SERVICE "_home-assistant"
#define HASS_MDNS_PROTO "_tcp"
#define HASS_MDNS_MAX_RESULTS 20
#define HASS_SCAN_TASK_STACK_SIZE 4096
#define HASS_SCAN_MIN_DURATION_SECONDS 1
#define HASS_SCAN_MAX_DURATION_SECONDS 30

/**
 * @brief Sequence number used to track WiFi scan requests.
 *
 * This variable increments or holds the sequence ID for WiFi scanning
 * operations to correlate requests and responses.
 */
static double scan_seq = 0;

typedef struct
{
    double seq;
    uint32_t duration_ms;
} hass_scan_task_context_t;

static TaskHandle_t hass_scan_task_handle = NULL;

/**
 * @brief Sequence number used to track WiFi switch requests.
 *
 * This variable holds the sequence ID for WiFi network switching operations,
 * used to synchronize request-response cycles.
 */
static double switch_wifi_seq = 0;
static int switch_wifi_socket_fd = -1;

/**
 * @brief Buffer to hold Base64-decoded initialization vector (IV).
 *
 * This buffer stores the raw IV data after decoding from Base64,
 * used for AES decryption operations.
 */
EXT_RAM_BSS_ATTR unsigned char iv_base64_decode[16] = {0};

/**
 * @brief Buffer to hold Base64-decoded encrypted content.
 *
 * This buffer stores the raw encrypted data after decoding from Base64,
 * prior to AES decryption.
 */
EXT_RAM_BSS_ATTR unsigned char content_base64_decode[200] = {0};

/**
 * @brief Buffer to hold decrypted content after AES decryption.
 *
 * This buffer contains the plaintext result of AES decryption operations.
 */
EXT_RAM_BSS_ATTR unsigned char content_decrypt[200] = {0};

/**
 * @brief Convert WiFi authentication mode enum to a human-readable string.
 *
 * @param mode WiFi authentication mode (wifi_auth_mode_t).
 * @return Pointer to a string describing the authentication mode.
 */
static const char *get_auth_mode_str(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN:
        return "None";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA PSK (TKIP)";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2 PSK (CCMP)";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "mixed WPA/WPA2 PSK (TKIP, CCMP)";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2 EAP (CCMP)";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3 SAE (CCMP)";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "mixed WPA2/WPA3 PSK, SAE (CCMP)";
    default:
        return "Unknown";
    }
}

/**
 * @brief Decrypt AES-256 encrypted Base64-encoded JSON content.
 *
 * This function performs AES-256 CBC decryption on Base64-encoded
 * data using a derived shared key from the nonce, and outputs
 * the decrypted plaintext JSON.
 *
 * @param base64_iv Base64-encoded initialization vector.
 * @param base64_content Base64-encoded encrypted content.
 * @param nonce Nonce string used to derive the shared key (via SHA-256).
 * @param output Buffer to store decrypted output; should be large enough.
 * @param output_size Size of the output buffer.
 *
 * @return ESP_OK on success, ESP_FAIL or ESP_ERR_INVALID_ARG on failure.
 */
static esp_err_t aes256_decrypt_base64_json(const char *base64_iv, const char *base64_content, const char *nonce,
                                            uint8_t *output)
{
    if (!base64_iv || !base64_content || !nonce || !output) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t share_key[32] = {0};
    uint8_t iv_bin[32] = {0};
    uint8_t content_bin[512] = {0};
    size_t iv_len = 0, content_len = 0;

    if (sha256_get((const uint8_t *)nonce, strlen(nonce), (char *)share_key) != ESP_OK) {
        ESP_LOGI(TAG, "sha256_get failed");
        return ESP_FAIL;
    }

    if (mbedtls_base64_decode(iv_bin, sizeof(iv_bin), &iv_len, (const uint8_t *)base64_iv, strlen(base64_iv)) != 0) {
        ESP_LOGE(TAG, "Base64 decode IV failed");
        return ESP_FAIL;
    }

    if (mbedtls_base64_decode(content_bin, sizeof(content_bin), &content_len, (const uint8_t *)base64_content,
                              strlen(base64_content)) != 0) {
        ESP_LOGE(TAG, "Base64 decode content failed");
        return ESP_FAIL;
    }

    if (iv_len != 16 || content_len == 0 ||
        content_len >= sizeof(content_bin) || (content_len % 16) != 0) {
        ESP_LOGE(TAG,
                 "Invalid AES input lengths: iv=%u content=%u",
                 (unsigned)iv_len,
                 (unsigned)content_len);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "AES input decoded: iv_len=%u content_len=%u",
             (unsigned)iv_len,
             (unsigned)content_len);

    esp_aes_context ctx;
    esp_aes_init(&ctx);
    if (esp_aes_setkey(&ctx, share_key, 256) != 0) {
        ESP_LOGE(TAG, "AES set_key failed");
        esp_aes_free(&ctx);
        return ESP_FAIL;
    }

    esp_err_t result = esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, content_len, iv_bin, content_bin, output);
    esp_aes_free(&ctx);

    if (result != 0) {
        ESP_LOGE(TAG, "AES decrypt failed: %s", esp_err_to_name(result));
        return ESP_FAIL;
    }

    output[content_len] = '\0';
    const uint8_t padding = output[content_len - 1];
    bool padding_valid = padding >= 1 && padding <= 16;
    if (padding_valid) {
        for (size_t i = 0; i < padding; i++) {
            if (output[content_len - 1 - i] != padding) {
                padding_valid = false;
                break;
            }
        }
    }
    ESP_LOGI(TAG,
             "AES decrypted: first_byte=0x%02X padding=%u padding_valid=%d",
             output[0],
             (unsigned)padding,
             padding_valid);
    return ESP_OK;
}

/**
 * @brief Handle API request to set the Access Point (AP) password.
 *
 * This function decrypts the incoming encrypted JSON payload containing
 * the new AP password, extracts the password, and updates the AP configuration.
 * It responds to all clients with the operation result.
 *
 * @param[in] data JSON object containing encrypted AP password information.
 * @param[in] now_resp_arg Pointer to asynchronous response argument containing client info.
 *
 * @return ESP_OK if password was successfully set, ESP_FAIL otherwise.
 */
esp_err_t method_api_handle_net_set_ap_pass(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error = 400;
    char *content_str = NULL;
    char *iv_str = NULL;
    uint8_t decrypted[512] = {0};
    char nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};

    cjson_parse_string_field(data, NETWORK_WIFI_CONTENT, &content_str);
    cjson_parse_string_field(data, NETWORK_WIFI_IV, &iv_str);
    webserver_get_client_nonce(now_resp_arg->fd, nonce);

    if (!content_str || !iv_str)
        goto exit;

    if (aes256_decrypt_base64_json(iv_str, content_str, nonce, decrypted) != ESP_OK)
        goto exit;

    ESP_LOGI(TAG, "Decrypted AP password JSON: %s", decrypted);

    cJSON *json_wifi_info = cJSON_Parse((char *)decrypted);
    if (!json_wifi_info)
        goto exit;

    cJSON *json_password = cJSON_GetObjectItemCaseSensitive(json_wifi_info, NETWORK_WIFI_PASSWORD);
    if (!json_password) {
        cJSON_Delete(json_wifi_info);
        goto exit;
    }
    network_sta_connect_params_t params = {0};
    if (strlen(json_password->valuestring) > sizeof(params.password) - 1) {
        cJSON_Delete(json_wifi_info);
        error = WEB_API_ERROR_PARAM_LEN;
        goto exit;
    }
    strncpy((char *)params.password, cJSON_GetStringValue(json_password), sizeof(params.password) - 1);
    cJSON_Delete(json_wifi_info);

    error = 0;
    webserver_code_response_to_all_client(error, WEB_API_METHOD_NET_SET_AP_PASS_RSP, now_resp_arg->seq);
    vTaskDelay(pdMS_TO_TICKS(500));
    network_wifi_ap_set_password(params.password);
    return ESP_OK;

exit:
    webserver_code_response_to_all_client(error, WEB_API_METHOD_NET_SET_AP_PASS_RSP, now_resp_arg->seq);
    return ESP_FAIL;
}

/**
 * @brief Handle API request to switch WiFi network connection.
 *
 * This function decrypts the received WiFi credentials, parses the SSID,
 * password, and BSSID from the decrypted JSON, and initiates a connection
 * to the specified WiFi access point.
 *
 * @param[in] data JSON object containing encrypted WiFi connection parameters.
 * @param[in] now_resp_arg Pointer to asynchronous response argument with client info.
 *
 * @return ESP_OK if the connection process started successfully, ESP_FAIL otherwise.
 */
esp_err_t method_api_handle_net_switch_wifi(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t decrypted[512] = {0};
    char nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};
    char *content_str = NULL;
    char *iv_str = NULL;

    cjson_parse_string_field(data, NETWORK_WIFI_CONTENT, &content_str);
    cjson_parse_string_field(data, NETWORK_WIFI_IV, &iv_str);
    switch_wifi_seq = now_resp_arg->seq;
    switch_wifi_socket_fd = now_resp_arg->fd;

    ESP_LOGI(TAG,
             "SwitchWiFi request: fd=%d seq=%.0f iv_b64_len=%u content_b64_len=%u",
             now_resp_arg->fd,
             now_resp_arg->seq,
             (unsigned)(iv_str ? strlen(iv_str) : 0),
             (unsigned)(content_str ? strlen(content_str) : 0));

    if (!content_str || !iv_str) {
        ESP_LOGE(TAG, "SwitchWiFi missing encrypted iv or content");
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_CONNECT_FAILED);
        return ret;
    }

    esp_err_t nonce_err = webserver_get_client_nonce(now_resp_arg->fd, nonce);
    if (nonce_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "SwitchWiFi failed to get nonce: fd=%d error=%s",
                 now_resp_arg->fd,
                 esp_err_to_name(nonce_err));
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_CONNECT_FAILED);
        return ret;
    }
    ESP_LOGI(TAG,
             "SwitchWiFi server nonce: fd=%d len=%u tag=%.8s",
             now_resp_arg->fd,
             (unsigned)strlen(nonce),
             nonce);

    esp_err_t decrypt_err = aes256_decrypt_base64_json(iv_str,
                                                       content_str,
                                                       nonce,
                                                       decrypted);
    if (decrypt_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "SwitchWiFi decrypt failed: fd=%d seq=%.0f error=%s",
                 now_resp_arg->fd,
                 now_resp_arg->seq,
                 esp_err_to_name(decrypt_err));
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_CONNECT_FAILED);
        return ret;
    }

    cJSON *json_wifi_info = cJSON_Parse((char *)decrypted);
    if (!json_wifi_info) {
        const char *parse_error = cJSON_GetErrorPtr();
        ptrdiff_t error_offset = -1;
        if (parse_error && parse_error >= (const char *)decrypted &&
            parse_error < (const char *)decrypted + sizeof(decrypted)) {
            error_offset = parse_error - (const char *)decrypted;
        }
        ESP_LOGE(TAG,
                 "SwitchWiFi decrypted JSON invalid: fd=%d seq=%.0f first_byte=0x%02X error_offset=%d; check nonce/key synchronization",
                 now_resp_arg->fd,
                 now_resp_arg->seq,
                 decrypted[0],
                 (int)error_offset);
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_CONNECT_FAILED);
        return ret;
    }

    ESP_LOGI(TAG,
             "SwitchWiFi decrypted JSON parsed: fd=%d seq=%.0f",
             now_resp_arg->fd,
             now_resp_arg->seq);

    cJSON *json_ssid = cJSON_GetObjectItemCaseSensitive(json_wifi_info, NETWORK_WIFI_SSID);
    cJSON *json_password = cJSON_GetObjectItemCaseSensitive(json_wifi_info, NETWORK_WIFI_PASSWORD);
    cJSON *json_bssid = cJSON_GetObjectItemCaseSensitive(json_wifi_info, NETWORK_WIFI_BSSID);

    if (!cJSON_IsString(json_ssid) || !json_ssid->valuestring ||
        !cJSON_IsString(json_password) || !json_password->valuestring ||
        !cJSON_IsString(json_bssid) || !json_bssid->valuestring) {
        ESP_LOGE(TAG,
                 "SwitchWiFi JSON fields invalid: ssid=%d password=%d bssid=%d",
                 cJSON_IsString(json_ssid),
                 cJSON_IsString(json_password),
                 cJSON_IsString(json_bssid));
        cJSON_Delete(json_wifi_info);
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_CONNECT_FAILED);
        return ESP_FAIL;
    }

    network_sta_connect_params_t wifi_params = {0};
    const size_t ssid_len = strlen(json_ssid->valuestring);
    const size_t password_len = strlen(json_password->valuestring);
    if (ssid_len > sizeof(wifi_params.ssid) - 1 ||
        password_len > sizeof(wifi_params.password) - 1) {
        ESP_LOGE(TAG,
                 "SwitchWiFi field too long: ssid_len=%u password_len=%u",
                 (unsigned)ssid_len,
                 (unsigned)password_len);
        cJSON_Delete(json_wifi_info);
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_PARAM_LEN);
        return ESP_FAIL;
    }
    if (password_len > 0 && password_len < 8) {
        ESP_LOGW(TAG, "SwitchWiFi password is shorter than the supported minimum");
        cJSON_Delete(json_wifi_info);
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_AUTH_FAILED);
        return ESP_FAIL;
    }
    strncpy((char *)wifi_params.ssid, cJSON_GetStringValue(json_ssid), sizeof(wifi_params.ssid) - 1);
    strncpy((char *)wifi_params.password, cJSON_GetStringValue(json_password), sizeof(wifi_params.password) - 1);

    uint8_t bssid[6] = {0};
    const int bssid_parts = sscanf(cJSON_GetStringValue(json_bssid),
                                   "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                                   &bssid[0],
                                   &bssid[1],
                                   &bssid[2],
                                   &bssid[3],
                                   &bssid[4],
                                   &bssid[5]);
    if (bssid_parts != 6) {
        ESP_LOGE(TAG,
                 "SwitchWiFi invalid BSSID format: parsed_parts=%d",
                 bssid_parts);
        cJSON_Delete(json_wifi_info);
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_CONNECT_FAILED);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "SwitchWiFi payload validated: ssid_len=%u password_len=%u",
             (unsigned)ssid_len,
             (unsigned)password_len);
    memcpy(wifi_params.bssid, bssid, sizeof(bssid));
    esp_err_t connect_err = network_wifi_sta_start_connect(wifi_params);
    if (connect_err != ESP_OK) {
        ESP_LOGE(TAG, "SwitchWiFi failed to start connection: %s", esp_err_to_name(connect_err));
        cJSON_Delete(json_wifi_info);
        webserver_network_wifi_connect_result_notify(WEB_API_ERROR_WIFI_CONNECT_FAILED);
        if (net_state.wifi_params.ssid[0] != 0) {
            network_wifi_sta_connect(&net_state.wifi_params);
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG,
             "SwitchWiFi connect dispatched: fd=%d seq=%.0f",
             now_resp_arg->fd,
             now_resp_arg->seq);
    cJSON_Delete(json_wifi_info);
    return ESP_OK;
}

/**
 * @brief Handle API request to start a WiFi scan.
 *
 * Checks if a scan is currently in progress and if not, initiates a WiFi scan
 * for the specified duration. Sends a response to all clients with the status
 * of the scan request.
 *
 * @param[in] data JSON object containing optional scan duration parameter.
 * @param[in] now_resp_arg Pointer to asynchronous response argument with client info.
 *
 * @return ESP_OK if the scan request was accepted, ESP_FAIL if a scan is already in progress.
 */
esp_err_t method_api_handle_net_scan(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t res = ESP_OK;
    int error_code = 0;
    if (network_get_if_wifi_scan()) {
        ESP_LOGW(TAG, "Wi-Fi scan already in progress");
        return ESP_FAIL;
    }

    cJSON *json_scan_duration = cJSON_GetObjectItemCaseSensitive(data, NETWORK_SCAN_DURATION);
    int duration = json_scan_duration ? (int)cJSON_GetNumberValue(json_scan_duration) : 3;
    network_wifi_scan(duration);
// exit:
    webserver_code_response_to_all_client(error_code, WEB_API_METHOD_NET_SCAN_RSP, now_resp_arg->seq);
    return res;
}

static bool hass_mdns_get_ipv4(const mdns_result_t *result, char *ip_buf, size_t ip_buf_len)
{
    for (const mdns_ip_addr_t *addr = result->addr; addr; addr = addr->next) {
        if (addr->addr.type == IPADDR_TYPE_V4) {
            return esp_ip4addr_ntoa(&addr->addr.u_addr.ip4, ip_buf, ip_buf_len) != NULL;
        }
    }
    return false;
}

static void hass_mdns_add_txt_value(cJSON *json, const mdns_result_t *result, const char *txt_key,
                                    const char *json_key)
{
    for (size_t i = 0; i < result->txt_count; ++i) {
        const mdns_txt_item_t *txt = &result->txt[i];
        if (!txt->key || !txt->value || strcmp(txt->key, txt_key) != 0) {
            continue;
        }

        size_t value_len = result->txt_value_len ? result->txt_value_len[i] : strlen(txt->value);
        char value[UINT8_MAX + 1];
        if (value_len > UINT8_MAX) {
            value_len = UINT8_MAX;
        }
        memcpy(value, txt->value, value_len);
        value[value_len] = '\0';
        cJSON_AddStringToObject(json, json_key, value);
        return;
    }
}

static cJSON *hass_mdns_result_to_json(const mdns_result_t *result)
{
    char ip_address[MDNS_IP_LEN] = {0};
    if (!hass_mdns_get_ipv4(result, ip_address, sizeof(ip_address))) {
        return NULL;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return NULL;
    }

    cJSON_AddStringToObject(json, "instanceName", result->instance_name ? result->instance_name : "");
    cJSON_AddStringToObject(json, "hostname", result->hostname ? result->hostname : "");
    cJSON_AddStringToObject(json, "ipAddress", ip_address);
    cJSON_AddNumberToObject(json, "port", result->port);
    hass_mdns_add_txt_value(json, result, "location_name", "locationName");
    hass_mdns_add_txt_value(json, result, "uuid", "uuid");
    hass_mdns_add_txt_value(json, result, "version", "version");
    hass_mdns_add_txt_value(json, result, "internal_url", "internalUrl");
    hass_mdns_add_txt_value(json, result, "external_url", "externalUrl");
    return json;
}

static bool hass_mdns_result_is_duplicate(const cJSON *hass_list_json, const cJSON *candidate)
{
    const cJSON *candidate_uuid = cJSON_GetObjectItemCaseSensitive(candidate, "uuid");
    const cJSON *candidate_ip = cJSON_GetObjectItemCaseSensitive(candidate, "ipAddress");
    const cJSON *candidate_port = cJSON_GetObjectItemCaseSensitive(candidate, "port");
    cJSON *existing = NULL;

    cJSON_ArrayForEach(existing, hass_list_json) {
        const cJSON *existing_uuid = cJSON_GetObjectItemCaseSensitive(existing, "uuid");
        if (cJSON_IsString(candidate_uuid) && candidate_uuid->valuestring && candidate_uuid->valuestring[0] != '\0' &&
            cJSON_IsString(existing_uuid) && existing_uuid->valuestring &&
            strcmp(candidate_uuid->valuestring, existing_uuid->valuestring) == 0) {
            return true;
        }

        const cJSON *existing_ip = cJSON_GetObjectItemCaseSensitive(existing, "ipAddress");
        const cJSON *existing_port = cJSON_GetObjectItemCaseSensitive(existing, "port");
        if (cJSON_IsString(candidate_ip) && candidate_ip->valuestring && cJSON_IsNumber(candidate_port) &&
            cJSON_IsString(existing_ip) && existing_ip->valuestring && cJSON_IsNumber(existing_port) &&
            strcmp(candidate_ip->valuestring, existing_ip->valuestring) == 0 &&
            candidate_port->valuedouble == existing_port->valuedouble) {
            return true;
        }
    }
    return false;
}

static void hass_instance_scan_task(void *param)
{
    hass_scan_task_context_t *context = (hass_scan_task_context_t *)param;
    double seq = context->seq;
    uint32_t duration_ms = context->duration_ms;
    free(context);

    mdns_result_t *results = NULL;
    esp_err_t scan_error =
        mdns_query_ptr(HASS_MDNS_SERVICE, HASS_MDNS_PROTO, duration_ms, HASS_MDNS_MAX_RESULTS, &results);

    cJSON *data_json = cJSON_CreateObject();
    cJSON *hass_list_json = cJSON_CreateArray();
    if (data_json && hass_list_json) {
        cJSON_AddItemToObject(data_json, "hassList", hass_list_json);

        size_t result_count = 0;
        size_t duplicate_count = 0;
        if (scan_error == ESP_OK) {
            for (mdns_result_t *result = results; result; result = result->next) {
                cJSON *result_json = hass_mdns_result_to_json(result);
                if (result_json) {
                    if (hass_mdns_result_is_duplicate(hass_list_json, result_json)) {
                        cJSON_Delete(result_json);
                        ++duplicate_count;
                        continue;
                    }
                    cJSON_AddItemToArray(hass_list_json, result_json);
                    ++result_count;
                }
            }
        }

        ESP_LOGI(TAG, "Home Assistant mDNS scan finished in %u ms: %u unique result(s), %u duplicate(s), error=%s",
                 (unsigned)duration_ms, (unsigned)result_count, (unsigned)duplicate_count,
                 esp_err_to_name(scan_error));
        webserver_data_response_to_all_client(data_json, WEB_API_METHOD_NET_HASS_INSTANCE_SCAN_RES_NOTIFY, seq);
    }
    else {
        ESP_LOGE(TAG, "Failed to allocate Home Assistant scan result JSON");
        cJSON_Delete(data_json);
        cJSON_Delete(hass_list_json);
    }

    mdns_query_results_free(results);
    hass_scan_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Start an asynchronous mDNS scan for Home Assistant instances.
 */
esp_err_t method_api_handle_net_scan_hass_instance(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error_code = WEB_API_ERROR_OK;
    uint32_t duration_seconds = MDNS_SCAN_TIME_MS / 1000;
    cJSON *json_scan_duration = data ? cJSON_GetObjectItemCaseSensitive(data, NETWORK_SCAN_DURATION) : NULL;

    if (json_scan_duration) {
        double duration = cJSON_GetNumberValue(json_scan_duration);
        if (!cJSON_IsNumber(json_scan_duration) || duration < HASS_SCAN_MIN_DURATION_SECONDS ||
            duration > HASS_SCAN_MAX_DURATION_SECONDS) {
            ESP_LOGW(TAG, "Invalid Home Assistant scan duration; expected an integer from %d to %d seconds",
                     HASS_SCAN_MIN_DURATION_SECONDS, HASS_SCAN_MAX_DURATION_SECONDS);
            error_code = WEB_API_ERROR_FAIL;
        }
        else {
            uint32_t requested_duration = (uint32_t)duration;
            if (duration != requested_duration) {
                ESP_LOGW(TAG, "Invalid Home Assistant scan duration; expected an integer from %d to %d seconds",
                         HASS_SCAN_MIN_DURATION_SECONDS, HASS_SCAN_MAX_DURATION_SECONDS);
                error_code = WEB_API_ERROR_FAIL;
            }
            else {
                duration_seconds = requested_duration;
            }
        }
    }

    if (error_code == WEB_API_ERROR_OK && !net_state.eth_connected && !net_state.sta_connected_flag) {
        ESP_LOGW(TAG, "Cannot scan Home Assistant instances without a LAN connection");
        error_code = WEB_API_ERROR_FAIL;
    }
    else if (error_code == WEB_API_ERROR_OK && hass_scan_task_handle != NULL) {
        ESP_LOGW(TAG, "Home Assistant mDNS scan is already running");
        error_code = WEB_API_ERROR_FAIL;
    }
    else if (error_code == WEB_API_ERROR_OK) {
        hass_scan_task_context_t *context = malloc(sizeof(*context));
        if (!context) {
            error_code = WEB_API_ERROR_FAIL;
        }
        else {
            context->seq = now_resp_arg->seq;
            context->duration_ms = duration_seconds * 1000;
            BaseType_t task_result = xTaskCreatePinnedToCore(hass_instance_scan_task, "HASS_SCAN",
                                                             HASS_SCAN_TASK_STACK_SIZE, context, 5,
                                                             &hass_scan_task_handle, 1);
            if (task_result != pdPASS) {
                ESP_LOGE(TAG, "Failed to create Home Assistant scan task");
                free(context);
                hass_scan_task_handle = NULL;
                error_code = WEB_API_ERROR_FAIL;
            }
        }
    }

    webserver_code_response_to_all_client(error_code, WEB_API_METHOD_NET_SCAN_HASS_INSTANCE_RSP, now_resp_arg->seq);
    return error_code == WEB_API_ERROR_OK ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Parse static IP configuration from JSON data.
 *
 * Extracts IP address, subnet mask, gateway, and DNS server addresses
 * from the provided JSON object and fills the output IP config structure.
 *
 * @param[in] data JSON object containing static IP configuration fields.
 * @param[out] out Pointer to a network_ip_config_t structure to be populated.
 *
 * @return true if all required fields are found and parsed successfully, false otherwise.
 */
static bool parse_static_ip_config(cJSON *data, network_ip_config_t *out)
{
    if (!data || !out)
        return false;

    cJSON *json_ip = cJSON_GetObjectItemCaseSensitive(data, NETWORK_IP_ADDRESS);
    cJSON *json_mask = cJSON_GetObjectItemCaseSensitive(data, NETWORK_SUBNET_MASK);
    cJSON *json_dns1 = cJSON_GetObjectItemCaseSensitive(data, NETWORK_DNS1);
    cJSON *json_dns2 = cJSON_GetObjectItemCaseSensitive(data, NETWORK_DNS2);
    cJSON *json_gateway = cJSON_GetObjectItemCaseSensitive(data, NETWORK_GATEWAY);

    const char *ip_text = cJSON_GetStringValue(json_ip);
    const char *mask_text = cJSON_GetStringValue(json_mask);
    const char *dns1_text = cJSON_GetStringValue(json_dns1);
    const char *dns2_text = cJSON_GetStringValue(json_dns2);
    const char *gateway_text = cJSON_GetStringValue(json_gateway);
    if (!ip_text || !mask_text || !dns1_text || !gateway_text)
        return false;

    ip4_addr_t ip = {0};
    ip4_addr_t netmask = {0};
    ip4_addr_t dns1 = {0};
    ip4_addr_t dns2 = {0};
    ip4_addr_t gateway = {0};
    if (!ip4addr_aton(ip_text, &ip) || ip4_addr_isany_val(ip) ||
        !ip4addr_aton(mask_text, &netmask) || ip4_addr_isany_val(netmask) ||
        !ip4addr_aton(dns1_text, &dns1) || ip4_addr_isany_val(dns1) ||
        !ip4addr_aton(gateway_text, &gateway) || ip4_addr_isany_val(gateway) ||
        (dns2_text && !ip4addr_aton(dns2_text, &dns2))) {
        return false;
    }

    out->ip = ip.addr;
    out->netmask = netmask.addr;
    out->dns1 = dns1.addr;
    out->dns2 = dns2.addr;
    out->gateway = gateway.addr;
    return true;
}

/**
 * @brief Handle API request to set Ethernet network configuration.
 *
 * Parses JSON data to configure the Ethernet interface IP acquisition mode (DHCP or static),
 * validates static IP configurations, checks for IP conflicts, and updates the network state.
 * Sends a response code to all clients to indicate success or failure of the operation.
 *
 * @param[in] data JSON object containing Ethernet configuration parameters.
 * @param[in] now_resp_arg Pointer to async response argument, including sequence tracking.
 *
 * @return ESP_OK on successful configuration, ESP_FAIL on error.
 */
esp_err_t method_api_handle_net_set_eth_config(cJSON *data, async_resp_arg *now_resp_arg)
{
    webserver_code_response_to_all_client(400, WEB_API_METHOD_NET_SET_ETH_CONFIG_RSP, now_resp_arg->seq);
    return ESP_FAIL;
}

/**
 * @brief Handle API request to set WiFi station configuration.
 *
 * Parses the provided JSON data to configure the WiFi station's IP acquisition mode
 * (DHCP or static) and updates the network state accordingly.
 * Validates static IP settings and checks for IP conflicts when necessary.
 * Sends a response code to all connected clients indicating success or error.
 *
 * @param[in] data JSON object containing the configuration parameters.
 * @param[in] now_resp_arg Pointer to the asynchronous response argument structure,
 *                         used to track request sequencing.
 *
 * @return ESP_OK if the configuration was applied successfully, or ESP_FAIL on failure.
 */
esp_err_t method_api_handle_net_set_wifi_config(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error_code = 0;
    char *ip_acquisition = NULL;
    network_ip_config_t ip_cfg = {0};
    network_ip_config_mode_t old_mode = net_state.sta_ip_mode;

    cJSON *json_ip_acquisition = cJSON_GetObjectItemCaseSensitive(data, NETWORK_IP_ACQUISITION);
    if (!json_ip_acquisition || !(ip_acquisition = cJSON_GetStringValue(json_ip_acquisition))) {
        error_code = 400;
        goto respond;
    }

    if (strcmp(ip_acquisition, "dhcp") == 0) {
        net_state.sta_ip_mode = IP_MODE_DHCP_AUTO;
    }
    else if (strcmp(ip_acquisition, "static") == 0) {
        net_state.sta_ip_mode = IP_MODE_STATIC;
        if (!parse_static_ip_config(data, &ip_cfg)) {
            error_code = 400;
            net_state.sta_ip_mode = old_mode;
            goto respond;
        }
    }
    else {
        error_code = 400;
        net_state.sta_ip_mode = old_mode;
        goto respond;
    }

    if (net_state.sta_ip_mode == IP_MODE_STATIC) {
        ip4_addr_t target_ip = {.addr = ip_cfg.ip};
        if (net_state.sta_connected_flag && network_check_ip_conflict(target_ip)) {
            error_code = 402;
            net_state.sta_ip_mode = old_mode;
            goto respond;
        }
    }

    webserver_code_response_to_all_client(error_code, WEB_API_METHOD_NET_SET_WIFI_CONFIG_RSP, now_resp_arg->seq);
    vTaskDelay(pdMS_TO_TICKS(500));
    network_wifi_sta_apply_ip_mode(ip_cfg);
    return ESP_OK;

respond:
    webserver_code_response_to_all_client(error_code, WEB_API_METHOD_NET_SET_WIFI_CONFIG_RSP, now_resp_arg->seq);
    return ESP_FAIL;
}

/**
 * @brief Handle API request to set Access Point (AP) configuration.
 *
 * Parses the provided JSON data to enable/disable the AP mode and
 * control internet connection sharing through the AP.
 * Synchronizes the configuration state and sends a response code to all clients.
 *
 * @param[in] data JSON object containing the AP configuration parameters.
 * @param[in] now_resp_arg Pointer to the asynchronous response argument structure,
 *                         used to track request sequencing.
 *
 * @return ESP_OK after processing the configuration request.
 */
esp_err_t method_api_handle_net_set_ap_config(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error_code = 0;
    int ap_channel = -1;
    int bandwidth = -1;
    bool ap_enable_flag;
    bool ap_default_enable_flag;
    bool ap_hide_ssid_flag;
    bool ap_internet_connection_enable_flag;
    bool runtime_config_changed = false;
    bool default_config_changed = false;
    char *ap_gateway_str_val = NULL;
    char *ap_subnet_mask_str_val = NULL;
    if (cJSON_HasObjectItem(data, NETWORK_AP_HIDE_SSID) &&
        cjson_parse_bool_field(data, NETWORK_AP_HIDE_SSID, &ap_hide_ssid_flag)) {
        net_state.ap_config.hide_ssid = ap_hide_ssid_flag;
        runtime_config_changed = true;
    }
    if (cJSON_HasObjectItem(data, NETWORK_AP_CHANNEL) &&
        cjson_parse_int_field(data, NETWORK_AP_CHANNEL, &ap_channel)) {
        net_state.ap_config.channel = ap_channel;
        runtime_config_changed = true;
    }
    if (cJSON_HasObjectItem(data, NETWORK_AP_BANDWIDTH) &&
        cjson_parse_int_field(data, NETWORK_AP_BANDWIDTH, &bandwidth)) {
        network_wifi_ap_set_bandwidth(bandwidth);
        runtime_config_changed = true;
    }
    if (cJSON_HasObjectItem(data, NETWORK_AP_GATEWAY) &&
        cJSON_HasObjectItem(data, NETWORK_AP_SUBNET_MASK) &&
        cjson_parse_string_field(data, NETWORK_AP_GATEWAY, &ap_gateway_str_val) &&
        cjson_parse_string_field(data, NETWORK_AP_SUBNET_MASK, &ap_subnet_mask_str_val)) {
        network_wifi_ap_set_gateway_netmask(ipaddr_addr(ap_gateway_str_val), ipaddr_addr(ap_subnet_mask_str_val));
        runtime_config_changed = true;
    }
    if (cJSON_HasObjectItem(data, NETWORK_AP_ENABLED) &&
        cjson_parse_bool_field(data, NETWORK_AP_ENABLED, &ap_enable_flag)) {
        /* An AP explicitly enabled from the console stays on until the user
         * explicitly disables it. */
        net_state.ap_config.active = ap_enable_flag;
        network_ap_set_user_default(ap_enable_flag);
        runtime_config_changed = true;
        // basic_config_ap_switch_sync(ap_enable_flag);
    }
    if (cJSON_HasObjectItem(data, NETWORK_AP_DEFAULT_ENABLED) &&
        cjson_parse_bool_field(data, NETWORK_AP_DEFAULT_ENABLED, &ap_default_enable_flag)) {
        network_ap_set_user_default(ap_default_enable_flag);
        default_config_changed = true;
        if (ap_default_enable_flag && !net_state.ap_config.active) {
            net_state.ap_config.active = true;
            runtime_config_changed = true;
        }
    }
    if (cJSON_HasObjectItem(data, NETWORK_AP_INTERNET) &&
        cjson_parse_bool_field(data, NETWORK_AP_INTERNET, &ap_internet_connection_enable_flag)) {
        net_state.ap_config.internet_connect = ap_internet_connection_enable_flag;
        runtime_config_changed = true;
        // basic_config_ap_internet_sync(ap_internet_connection_enable_flag);
    }
    if (runtime_config_changed) {
        network_wifi_ap_set_active(net_state.ap_config.active);
    }
    else if (default_config_changed) {
        network_save_config_to_file(net_state);
        webserver_network_mode_info_notify(NET_MODE_AP_ONLY);
    }
    webserver_code_response_to_all_client(error_code, WEB_API_METHOD_NET_SET_AP_CONFIG_RSP, now_resp_arg->seq);
    return ESP_OK;
}

esp_err_t method_api_handle_net_set_ap_ssid(cJSON *data, async_resp_arg *now_resp_arg)
{
    char *ssid = NULL;
    if (cjson_parse_string_field(data, NETWORK_AP_SSID, &ssid)) {
        if (strlen(ssid) > sizeof(net_state.ap_params.ssid) - 1) {
            webserver_code_response_to_all_client(WEB_API_ERROR_PARAM_LEN, WEB_API_METHOD_NET_SET_AP_SSID_RSP,
                                                  now_resp_arg->seq);
            return ESP_OK;
        }
    }
    webserver_code_response_to_all_client(WEB_API_ERROR_OK, WEB_API_METHOD_NET_SET_AP_SSID_RSP, now_resp_arg->seq);
    vTaskDelay(pdMS_TO_TICKS(500));
    network_wifi_ap_set_ssid(ssid);
    return ESP_OK;
}

/**
 * @brief Add IP configuration details to a JSON object.
 *
 * Converts the IP configuration fields (IP address, subnet mask, gateway, DNS servers)
 * into string format and adds them to the provided cJSON object.
 *
 * @param[in,out] root Pointer to the JSON object to which IP info will be added.
 * @param[in] ip Pointer to the network_ip_config_t struct containing IP configuration.
 */
static void add_ip_info_to_json(cJSON *root, const network_ip_config_t *ip)
{
    ip4_addr_t addr = {0};

    addr.addr = ip->ip;
    cJSON_AddStringToObject(root, "ipAddress", ip4addr_ntoa(&addr));

    addr.addr = ip->netmask;
    cJSON_AddStringToObject(root, "subnetMask", ip4addr_ntoa(&addr));

    addr.addr = ip->gateway;
    cJSON_AddStringToObject(root, "gateway", ip4addr_ntoa(&addr));

    addr.addr = ip->dns1;
    cJSON_AddStringToObject(root, "dns1", ip4addr_ntoa(&addr));

    addr.addr = ip->dns2;
    cJSON_AddStringToObject(root, "dns2", ip4addr_ntoa(&addr));
}

/**
 * @brief Add MAC address string to a JSON object under a given key.
 *
 * Reads the MAC address of the specified type, formats it as a human-readable
 * string, and adds it to the JSON object with the provided key.
 *
 * @param[in,out] root Pointer to the JSON object.
 * @param[in] mac_type Type of MAC address to read (e.g., ESP_MAC_ETH, ESP_MAC_WIFI_STA).
 * @param[in] key JSON key under which to store the MAC address string.
 */
static void add_mac_address_to_json(cJSON *root, esp_mac_type_t mac_type, const char *key)
{
    uint8_t mac[6];
    esp_read_mac(mac, mac_type);
    char mac_str[20];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, key, mac_str);
}

/**
 * @brief Convert network IP configuration mode to a descriptive string.
 *
 * Maps the enum network_ip_config_mode_t values to string literals used in JSON.
 *
 * @param[in] mode IP configuration mode enum value.
 * @return String representing the IP mode ("dhcp" or "static").
 */
static const char *get_ip_mode_str(network_ip_config_mode_t mode)
{
    return mode == IP_MODE_STATIC ? "static" : "dhcp";
}

/**
 * @brief Create a JSON object containing Ethernet network information.
 *
 * Includes connection status, DHCP/static mode, IP address details,
 * and MAC address information.
 *
 * @param[out] eth_json Pointer to a cJSON pointer that will receive the created JSON object.
 * @return ESP_OK on success, or ESP_FAIL if JSON creation fails.
 */
static int network_create_eth_info_json(cJSON **eth_json)
{
    NetworkState *state = network_get_state();
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root for ETH");
        return ESP_FAIL;
    }

    cJSON_AddBoolToObject(root, "connected", state->eth_connected);
    cJSON_AddBoolToObject(root, "dhcpEnabled", state->eth_ip_mode == IP_MODE_DHCP_AUTO);
    cJSON_AddStringToObject(root, "ipAcquisition", get_ip_mode_str(state->eth_ip_mode));
    cJSON_AddNumberToObject(root, "ipAddconnectionSpeedress", 0);

    if (state->eth_ip_mode == IP_MODE_STATIC)
        add_ip_info_to_json(root, &state->eth_static_ip);
    else
        add_ip_info_to_json(root, &state->current_ip);

    add_mac_address_to_json(root, ESP_MAC_ETH, "macAddress");
    *eth_json = root;
    return ESP_OK;
}

/**
 * @brief Create a JSON object containing WiFi station (STA) network information.
 *
 * Includes connection status, SSID, security type, DHCP/static mode,
 * IP address details, MAC address, and WiFi channel.
 *
 * @param[out] wlan_json Pointer to a cJSON pointer that will receive the created JSON object.
 * @return ESP_OK on success, or ESP_FAIL if JSON creation fails.
 */
static int network_create_sta_info_json(cJSON **wlan_json)
{
    NetworkState *state = network_get_state();
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root for STA");
        return ESP_FAIL;
    }

    wifi_ap_record_t ap_info = {0};
    esp_wifi_sta_get_ap_info(&ap_info);
    cJSON_AddNumberToObject(root, "channel", ap_info.primary);
    cJSON_AddStringToObject(root, "security", get_auth_mode_str(ap_info.authmode));
    cJSON_AddBoolToObject(root, "connected", state->sta_connected_flag);
    cJSON_AddStringToObject(root, "ssid", (char *)state->wifi_params.ssid);
    cJSON_AddBoolToObject(root, "dhcpEnabled", state->sta_ip_mode == IP_MODE_DHCP_AUTO);
    cJSON_AddStringToObject(root, "ipAcquisition", get_ip_mode_str(state->sta_ip_mode));
    if (state->sta_ip_mode == IP_MODE_STATIC)
        add_ip_info_to_json(root, &state->sta_static_ip);
    else
        add_ip_info_to_json(root, &state->current_ip);

    add_mac_address_to_json(root, ESP_MAC_WIFI_STA, "macAddress");
    cJSON_AddNumberToObject(root, "ipAddconnectionSpeedress", 0);

    *wlan_json = root;
    return ESP_OK;
}

/**
 * @brief Create a JSON object with Access Point (AP) network information.
 *
 * Gathers AP status, configuration, connected clients, and network details,
 * then constructs a JSON object representing this AP information.
 *
 * @param[out] ap_json Pointer to a cJSON pointer where the created JSON object will be stored.
 *
 * @return ESP_OK on success, or ESP_FAIL if JSON object creation fails.
 */
static int network_create_ap_info_json(cJSON **ap_json)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root for AP");
        return ESP_FAIL;
    }

    cJSON_AddBoolToObject(root, "enabled", net_state.ap_config.active);
    cJSON_AddBoolToObject(root, "defaultEnabled", net_state.ap_config.default_active);
    cJSON_AddBoolToObject(root, "internetThroughAp", net_state.ap_config.internet_connect);
    cJSON_AddBoolToObject(root, "hideSsid", net_state.ap_config.hide_ssid);
    cJSON_AddNumberToObject(root, "bandwidth", (double)net_state.ap_config.band_width);

    cJSON_AddStringToObject(root, "apName", (char *)net_state.ap_params.ssid);
    cJSON_AddStringToObject(root, "password", (char *)net_state.ap_params.password);

    const char *security = (!net_state.ap_params.password[0] || strlen((char *)net_state.ap_params.password) < 8)
                               ? "None"
                               : "WPA2 PSK (CCMP)";
    cJSON_AddStringToObject(root, "security", security);

    wifi_config_t conf = {0};
    esp_wifi_get_config(WIFI_IF_AP, &conf);

    // int channel = conf.ap.channel;
    // if (net_state.sta_connected_flag) {
    //     wifi_ap_record_t ap_info = {0};
    //     if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    //         channel = ap_info.primary;
    // }
    cJSON_AddNumberToObject(root, "channel", net_state.ap_config.channel);

    wifi_sta_list_t sta_list = {0};
    esp_wifi_ap_get_sta_list(&sta_list);
    // basic_config_ap_device_num_sync(sta_list.num);

    cJSON *clients = cJSON_CreateObject();
    cJSON_AddNumberToObject(clients, "current", sta_list.num);
    cJSON_AddNumberToObject(clients, "max", conf.ap.max_connection);
    cJSON_AddItemToObject(root, "clients", clients);

    esp_netif_ip_info_t ip_info = {0};
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    cJSON_AddStringToObject(root, "gateway", ip4addr_ntoa((ip4_addr_t *)&ip_info.ip));
    cJSON_AddStringToObject(root, "subnetMask", ip4addr_ntoa((ip4_addr_t *)&ip_info.netmask));

    *ap_json = root;
    return ESP_OK;
}

/**
 * @brief Create a JSON array describing the current network mode(s).
 *
 * Converts a network_mode_t enum value into a JSON array of strings representing
 * active network modes (e.g., "AP", "Ethernet", "WiFi").
 *
 * @param[in] mode The current network mode to represent.
 *
 * @return Pointer to the created cJSON array on success, or NULL on failure.
 */
static cJSON *create_network_mode_json(network_mode_t mode)
{
    cJSON *mode_json = cJSON_CreateArray();
    if (!mode_json)
        return NULL;

    switch (mode) {
    case NET_MODE_AP_ONLY:
        cJSON_AddItemToArray(mode_json, cJSON_CreateString("AP"));
        break;
    case NET_MODE_ETHERNET_ONLY:
        cJSON_AddItemToArray(mode_json, cJSON_CreateString("Ethernet"));
        break;
    case NET_MODE_WLAN_ONLY:
        cJSON_AddItemToArray(mode_json, cJSON_CreateString("WiFi"));
        break;
    case NET_MODE_ETHERNET_AP:
        cJSON_AddItemToArray(mode_json, cJSON_CreateString("Ethernet"));
        cJSON_AddItemToArray(mode_json, cJSON_CreateString("AP"));
        break;
    case NET_MODE_WLAN_AP:
        cJSON_AddItemToArray(mode_json, cJSON_CreateString("WiFi"));
        cJSON_AddItemToArray(mode_json, cJSON_CreateString("AP"));
        break;
    default:
        break;
    }
    return mode_json;
}

/**
 * @brief Get the current network operating mode.
 *
 * Determines the current network mode based on the Ethernet connection status,
 * WiFi station connection status, and whether the Access Point mode is active.
 *
 * The possible returned modes include:
 * - Ethernet only
 * - Ethernet with AP
 * - WLAN only
 * - WLAN with AP
 * - AP only
 *
 * @return The current network mode as a value of type network_mode_t.
 */
network_mode_t network_get_current_mode(void)
{
    if (net_state.eth_connected) {
        net_state.current_mode = net_state.ap_config.active ? NET_MODE_ETHERNET_AP : NET_MODE_ETHERNET_ONLY;
    }
    else if (net_state.sta_connected_flag) {
        net_state.current_mode = net_state.ap_config.active ? NET_MODE_WLAN_AP : NET_MODE_WLAN_ONLY;
    }
    else {
        net_state.current_mode = NET_MODE_AP_ONLY;
    }
    return net_state.current_mode;
}

/**
 * @brief Notify all connected clients with detailed current network information.
 *
 * Creates a JSON object containing the current network mode, internet connectivity status,
 * mDNS domain, and detailed info for Ethernet, WiFi station, and access point interfaces.
 * This information is broadcast to all connected clients.
 *
 * The function also triggers synchronization of the network mode configuration.
 */
void webserver_network_info_notify(void)
{
    NetworkState *network_state = network_get_state();
    cJSON *data_json = cJSON_CreateObject();
    if (!data_json)
        return;

    cJSON_AddItemToObject(data_json, NETWORK_MODE, create_network_mode_json(network_get_current_mode()));
    cJSON_AddBoolToObject(data_json, INTERNET_CON, network_state->internet_connected);

    char domain[75] = {0};
    char host_name[65] = {0};
    mdns_hostname_get(host_name);
    snprintf(domain, sizeof(domain), "%s.local", host_name);
    cJSON_AddStringToObject(data_json, "mdnsDomain", domain);

    cJSON *eth_info = NULL;
    cJSON *wifi_info = NULL;
    cJSON *ap_info = NULL;

    network_create_eth_info_json(&eth_info);
    network_create_sta_info_json(&wifi_info);
    network_create_ap_info_json(&ap_info);
    cJSON_AddItemToObject(data_json, NETWORK_ETHERNET, eth_info);
    cJSON_AddItemToObject(data_json, NETWORK_WIFI, wifi_info);
    cJSON_AddItemToObject(data_json, NETWORK_AP, ap_info);
    webserver_data_response_to_all_client(data_json, WEB_API_METHOD_NET_INFO_NOTIFY, 0);
    // basic_config_network_mode_sync(network_get_current_mode());
}

/**
 * @brief Notify all connected clients about the current network mode and its details.
 *
 * Constructs a JSON object containing the current network mode, internet connectivity status,
 * and mode-specific network information (Ethernet, WiFi station, or access point).
 * Then broadcasts this information to all connected web clients.
 *
 * @param[in] current_mode The current network mode (e.g., Ethernet only, WLAN only, AP only).
 *
 * @note The function also triggers synchronization of the network mode configuration.
 */
void webserver_network_mode_info_notify(int current_mode)
{
    NetworkState *network_state = network_get_state();
    cJSON *data_json = cJSON_CreateObject();
    if (!data_json)
        return;

    cJSON_AddItemToObject(data_json, NETWORK_MODE, create_network_mode_json(network_get_current_mode()));
    cJSON_AddBoolToObject(data_json, INTERNET_CON, network_state->internet_connected);

    /* Keep incremental network updates consistent with the full network info
     * notification. In particular, the mDNS handler may append a MAC suffix
     * after STA obtains an address to avoid a hostname conflict. */
    char domain[75] = {0};
    char host_name[65] = {0};
    mdns_hostname_get(host_name);
    snprintf(domain, sizeof(domain), "%s.local", host_name);
    cJSON_AddStringToObject(data_json, "mdnsDomain", domain);

    cJSON *eth_info = NULL;
    cJSON *wifi_info = NULL;
    cJSON *ap_info = NULL;

    switch (current_mode) {
    case NET_MODE_ETHERNET_ONLY:
        network_create_eth_info_json(&eth_info);
        cJSON_AddItemToObject(data_json, NETWORK_ETHERNET, eth_info);
        break;
    case NET_MODE_WLAN_ONLY:
        network_create_sta_info_json(&wifi_info);
        cJSON_AddItemToObject(data_json, NETWORK_WIFI, wifi_info);
        break;
    case NET_MODE_AP_ONLY:
        network_create_ap_info_json(&ap_info);
        cJSON_AddItemToObject(data_json, NETWORK_AP, ap_info);
        break;
    default:
        break;
    }
    webserver_data_response_to_all_client(data_json, WEB_API_METHOD_NET_INFO_NOTIFY, 0);
    // basic_config_network_mode_sync(network_get_current_mode());
}

/**
 * @brief Notify all connected clients of the WiFi connection attempt result.
 *
 * Sends a response code indicating success or error of a WiFi connection attempt
 * to all connected clients.
 *
 * @param[in] error Integer error code representing the WiFi connection result.
 */
void webserver_network_wifi_connect_result_notify(int error)
{
    if (switch_wifi_socket_fd < 0) {
        webserver_code_response_to_all_client(error, WEB_API_METHOD_NET_SWITCH_WIFI_RSP, switch_wifi_seq);
        return;
    }

    cJSON *data = NULL;
    if (error == WEB_API_ERROR_OK) {
        char ticket[PROVISIONING_HANDOFF_TICKET_HEX_LEN + 1] = {0};
        if (provisioning_handoff_issue(switch_wifi_socket_fd, ticket) == ESP_OK) {
            data = cJSON_CreateObject();
            if (data) {
                cJSON_AddStringToObject(data, "handoffTicket", ticket);
                cJSON_AddNumberToObject(data, "expiresIn", 120);
            }
        }
        memset(ticket, 0, sizeof(ticket));
    }
    webserver_data_code_response_to_special_client_not_log(data,
                                                            WEB_API_METHOD_NET_SWITCH_WIFI_RSP,
                                                            switch_wifi_seq,
                                                            error,
                                                            switch_wifi_socket_fd);
    switch_wifi_socket_fd = -1;
}

/**
 * @brief Notify all connected clients with WiFi scan results.
 *
 * This function processes a list of scanned WiFi access points, converts the
 * information into a JSON format, and sends a notification containing the scan
 * results to all connected web clients.
 *
 * @param[in] ap_list Pointer to an array of wifi_ap_record_t structures representing the scanned access points.
 *                    Must not be NULL.
 * @param[in] apCount Number of access points in the ap_list array.
 *
 * @note If ap_list is NULL or JSON object creation fails, the function logs an error and returns without sending
 * notifications.
 */
void webserver_network_wifi_scan_info_notify(wifi_ap_record_t *ap_list, uint16_t apCount)
{
    if (!ap_list) {
        ESP_LOGE(TAG, "ap_list null error");
        return;
    }

    cJSON *data_json = cJSON_CreateObject();
    cJSON *results_json = cJSON_CreateArray();
    if (!data_json || !results_json) {
        ESP_LOGE(TAG, "JSON object creation failed");
        cJSON_Delete(data_json); // safe even if NULL
        return;
    }

    cJSON_AddItemToObject(data_json, "results", results_json);

    for (int i = 0; i < apCount; ++i) {
        cJSON *wifi_info = cJSON_CreateObject();
        if (!wifi_info) {
            ESP_LOGW(TAG, "Failed to create wifi_info object for AP %d", i);
            continue;
        }

        char bssid_str[20];
        snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X", ap_list[i].bssid[0],
                 ap_list[i].bssid[1], ap_list[i].bssid[2], ap_list[i].bssid[3], ap_list[i].bssid[4],
                 ap_list[i].bssid[5]);

        cJSON_AddStringToObject(wifi_info, "ssid", (char *)ap_list[i].ssid);
        cJSON_AddStringToObject(wifi_info, "bssid", bssid_str);
        cJSON_AddNumberToObject(wifi_info, "channel", ap_list[i].primary);
        cJSON_AddNumberToObject(wifi_info, "rssi", ap_list[i].rssi);
        cJSON_AddStringToObject(wifi_info, "security", get_auth_mode_str(ap_list[i].authmode));

        cJSON_AddItemToArray(results_json, wifi_info);
    }
    webserver_data_response_to_all_client(data_json, WEB_API_METHOD_NET_WIFI_SCAN_RES_NOTIFY, scan_seq);
}
