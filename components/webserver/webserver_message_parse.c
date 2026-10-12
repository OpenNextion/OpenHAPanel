#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include "aes/esp_aes.h"
#include "mbedtls/base64.h"
#include "webserver_config.h"
#include "webserver_method_api.h"
#include "webserver_auth.h"
#include "webserver_message_parse.h"
#include "cjson_helpers.h"
#include "webserver_config_forget_password.h"
#include "webserver_config_backup.h"
#include "webserver_config_api.h"
#include "webserver_firmware_update.h"
#include "webserver_screen.h"
#include "network_api.h"
#include "screen_api.h"
#include "screen_state.h"
#include "ha_auth_api.h"
#include "login_session.h"
#include "login_session_api.h"
#include "provisioning_handoff.h"
#include "sys_time.h"
#include "bsp_lcd.h"
#include "mdns_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "screen_night_mode.h"


static const char *TAG = "webserver_message_parse";

#define LOG_LEN_MAX 256
#define NIGHT_MODE_FROM_LEN 10
#define NIGHT_MODE_TO_LEN 10

static bool payload_contains(const uint8_t *payload, size_t len, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (!payload || needle_len == 0 || len < needle_len) {
        return false;
    }
    for (size_t i = 0; i <= len - needle_len; i++) {
        if (memcmp(payload + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static char *webserver_api_whitelist[] = {
    WEB_API_METHOD_SYS_SET_PASSWORD, WEB_API_METHOD_SYS_SET_LOGIN_STATE, WEB_API_METHOD_SYS_SET_FORGOT_PASSWORD,
    WEB_API_METHOD_SYS_RESUME_SESSION, WEB_API_METHOD_SYS_EXCHANGE_PROVISIONING_HANDOFF,
    WEB_API_METHOD_SET_THEME_MODE,WEB_API_METHOD_SET_LANGUAGE
};

/**
 * @brief  method api list
 */
static webserver_method_api_strategy webserver_method_api_list[] = {
    /*-- Method: Login -- */
    {WEB_API_METHOD_SYS_SET_PASSWORD, method_api_handle_set_password},
    {WEB_API_METHOD_SYS_SET_LOGIN_STATE, method_api_handle_set_login_state},
    {WEB_API_METHOD_SYS_SET_FORGOT_PASSWORD, method_api_handle_set_forgot_password},
    {WEB_API_METHOD_SYS_RESUME_SESSION, method_api_handle_resume_session},
    {WEB_API_METHOD_SYS_EXCHANGE_PROVISIONING_HANDOFF, method_api_handle_exchange_provisioning_handoff},
    {WEB_API_METHOD_SYS_LOGOUT, method_api_handle_logout},
    {WEB_API_METHOD_SYS_REVOKE_ALL_SESSIONS, method_api_handle_revoke_all_sessions},

    /* -- Method: Network -- */
    {WEB_API_METHOD_NET_SCAN, method_api_handle_net_scan},
    {WEB_API_METHOD_NET_SCAN_HASS_INSTANCE, method_api_handle_net_scan_hass_instance},
    {WEB_API_METHOD_NET_SWITCH_WIFI, method_api_handle_net_switch_wifi},
    {WEB_API_METHOD_NET_SET_WIFI_CONFIG, method_api_handle_net_set_wifi_config},
    {WEB_API_METHOD_NET_SET_AP_PASS, method_api_handle_net_set_ap_pass},
    {WEB_API_METHOD_NET_SET_AP_CONFIG, method_api_handle_net_set_ap_config},
    {WEB_API_METHOD_NET_SET_AP_SSID, method_api_handle_net_set_ap_ssid},
    {WEB_API_METHOD_NET_SET_MDNS_DOMAIN,method_api_handle_set_mdns_domain},

    /* -- Method: Home Assistant -- */
    {WEB_API_METHOD_HASS_START_OAUTH, method_api_handle_hass_start_oauth},
    {WEB_API_METHOD_HASS_COMPLETE_OAUTH, method_api_handle_hass_complete_oauth},
    {WEB_API_METHOD_HASS_GET_CONNECTION_INFO, method_api_handle_hass_get_connection_info},
    {WEB_API_METHOD_HASS_UPDATE_URL, method_api_handle_hass_update_url},
    {WEB_API_METHOD_HASS_DISABLE, method_api_handle_hass_disable},
    {WEB_API_METHOD_HASS_ACTIVATE, method_api_handle_hass_activate},
    {WEB_API_METHOD_HASS_REMOVE_AUTH, method_api_handle_hass_remove_authorization},

    /* -- Method: System-- */
    {WEB_API_METHOD_SET_THEME_MODE, method_api_handle_set_theme_mode},
    {WEB_API_METHOD_SET_SCREEN_CONFIG, method_api_handle_set_screen_config},
    {WEB_API_METHOD_SET_LANGUAGE, method_api_handle_set_language},
    {WEB_API_METHOD_SET_BRIGHTNESS, method_api_handle_set_brightness},
    {WEB_API_METHOD_SET_NIGHT_MODE, method_api_handle_set_night_mode},
    {WEB_API_METHOD_SET_SCREEN_LOCK, method_api_handle_set_screen_lock},
    {WEB_API_METHOD_SET_NAME, method_api_handle_set_name},
    {WEB_API_METHOD_DOWNLOAD_BACKUP, method_api_handle_download_backup},
    {WEB_API_METHOD_UPLOAD_BACKUP, method_api_handle_upload_backup},
    {WEB_API_METHOD_REBOOT_DEVICE, method_api_handle_reboot_device},
    {WEB_API_METHOD_RESET_DEVICE, method_api_handle_reset_device},

    /* -- Method: Time -- */
    {WEB_API_METHOD_SET_TIMEZONE, method_api_handle_set_timezone},
    {WEB_API_METHOD_GET_TIMEZONE, method_api_handle_get_timezone},
    {WEB_API_METHOD_SET_NTP_SERVER, method_api_handle_set_ntp_server},

    /* -- Method: Security. -- */
    {WEB_API_METHOD_SECU_SET_IP_WHITE_LIST, method_api_handle_set_ip_white_list},
};

static bool webserver_message_auth_check(char *method, int sock_fd)
{
    if (webserver_client_get_auth(sock_fd)) {
        return true;
    }
    else {
        for (int i = 0; i < sizeof(webserver_api_whitelist) / sizeof(webserver_api_whitelist[0]); ++i) {
            if (strcmp(webserver_api_whitelist[i], method) == 0) {
                return true;
            }
        }
    }
    return false;
}

static void webserver_message_auth_response(char *method, int error_code, double seq)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "message", "Unauthorized");
    char method_rsp[128];
    snprintf(method_rsp, sizeof(method_rsp), "%sRsp", method);
    webserver_data_code_response_to_current_client(data, method_rsp, seq, error_code);
}

static void webserver_message_ota_blocked_response(const char *method, double seq)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return;
    }
    cJSON_AddStringToObject(data, "message", "Firmware update in progress");

    char method_rsp[128];
    snprintf(method_rsp, sizeof(method_rsp), "%sRsp", method);
    webserver_data_code_response_to_current_client(data,
                                                   method_rsp,
                                                   seq,
                                                   WEB_API_ERROR_OTA_IS_DOING);
}

/**
 * @brief Handle incoming webserver data messages.
 *
 * This function processes a raw JSON payload received by the webserver.
 * It performs the following steps:
 * - Validates the payload and logs the message content (truncating if too long).
 * - Parses the JSON payload and extracts the 'data', 'seq', and 'method' fields.
 * - Retrieves the async response argument and assigns the sequence number.
 * - Dispatches the request to the appropriate API handler based on the 'method' field.
 * - Cleans up allocated resources before returning.
 *
 * @param payload  Pointer to the incoming JSON message buffer. Ownership is transferred to this function.
 * @param len      Length of the payload in bytes.
 */
void webserver_data_message_handler(uint8_t *payload, size_t len)
{
    async_resp_arg *now_resp_arg = websocket_get_async_rsp_arg();
    if (payload == NULL) {
        ESP_LOGE(TAG, "payload null error");
        return;
    }
    if (payload_contains(payload, len, WEB_API_METHOD_HASS_COMPLETE_OAUTH) ||
        payload_contains(payload, len, "\"code\"") ||
        payload_contains(payload, len, WEB_API_METHOD_SYS_SET_LOGIN_STATE) ||
        payload_contains(payload, len, WEB_API_METHOD_SYS_RESUME_SESSION) ||
        payload_contains(payload, len, WEB_API_METHOD_SYS_EXCHANGE_PROVISIONING_HANDOFF) ||
        payload_contains(payload, len, "\"proof\"")) {
        ESP_LOGI(TAG, "from fd: %d sensitive authentication message omitted", now_resp_arg->fd);
    }
    else if (len > LOG_LEN_MAX) {
        char *log_buf = (char *)calloc(1, LOG_LEN_MAX + 1);
        if (log_buf) {
            strncpy(log_buf, (char *)payload, LOG_LEN_MAX);
            ESP_LOGI(TAG, "from fd: %d message[%d/%d]=>%s", now_resp_arg->fd, LOG_LEN_MAX, len, log_buf);
            free(log_buf);
        }
    }
    else {
        ESP_LOGI(TAG, "from fd: %d message[%d]=>%s", now_resp_arg->fd, len, payload);
    }

    cJSON *json_root = cJSON_Parse((char *)payload);
    if (json_root == NULL) {
        ESP_LOGE(TAG, "webserver request json parse error");
        goto exit;
    }

    cJSON *json_data = cJSON_GetObjectItemCaseSensitive(json_root, WEB_API_KEY_DATA);
    if (json_data == NULL) {
        ESP_LOGE(TAG, "webserver request json missing data");
        goto exit;
    }

    cJSON *json_seq = cJSON_GetObjectItemCaseSensitive(json_root, WEB_API_KEY_SEQ);
    if (json_seq == NULL || !cJSON_IsNumber(json_seq)) {
        ESP_LOGE(TAG, "webserver request json missing req");
        goto exit;
    }

    cJSON *json_method = cJSON_GetObjectItemCaseSensitive(json_root, WEB_API_KEY_METHOD);
    if (!json_method || !cJSON_IsString(json_method)) {
        ESP_LOGE(TAG, "webserver request json missing method");
        goto exit;
    }

    now_resp_arg->seq = json_seq->valuedouble;

    char *method = json_method->valuestring;
    if (!webserver_message_auth_check(method, now_resp_arg->fd)) {
        webserver_message_auth_response(method, WEB_API_ERROR_AUTH, now_resp_arg->seq);
        goto exit;
    }
    if (webserver_firmware_update_operations_blocked() &&
        !webserver_firmware_update_method_allowed(method)) {
        ESP_LOGW(TAG,
                 "Rejected WebSocket method during firmware update: %s",
                 method);
        webserver_message_ota_blocked_response(method, now_resp_arg->seq);
        goto exit;
    }
    const size_t method_len = strlen(method);

    for (int i = 0; i < sizeof(webserver_method_api_list) / sizeof(webserver_method_api_strategy); ++i) {
        if (method_len == strlen(webserver_method_api_list[i].method) &&
            strncmp(webserver_method_api_list[i].method, method, method_len) == 0) {
            webserver_method_api_list[i].handler(json_data, now_resp_arg);
            break;
        }
    }

    // if (method) {
    //     if (strncmp(WEB_API_METHOD_REMOTE, method, strlen(WEB_API_METHOD_REMOTE)) == 0) {
    //         light_smart_api_handle(method, (char *)payload);
    //     }
    // }

exit:
    free(payload);

    if (json_root) {
        cJSON_Delete(json_root);
    }
}

/**
 * @brief Decrypt and parse password from incoming cJSON data.
 *
 * This function extracts the encrypted password content (`content`) and IV (`iv`) from the provided cJSON object,
 * decodes them from base64, and decrypts the password using AES-CBC with a shared key derived from the client's nonce.
 * The decrypted password is then parsed from the decrypted JSON string and stored in the provided output buffer.
 *
 * @param root           The root cJSON object containing "iv" and "content" fields.
 * @param password       Output buffer to store the decrypted password (must be at least 64 bytes).
 * @param password_len   Output length of the decrypted password.
 * @param resp_arg       Asynchronous response argument containing the client's file descriptor.
 *
 * @return ESP_OK on success, or an appropriate ESP_FAIL on error.
 */
esp_err_t parse_set_password(cJSON *root, char *password, size_t *password_len, int sock_fd)
{
    if (!root || !password || !password_len) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;
    cJSON *json_root = NULL;
    esp_aes_context ctx;
    bool aes_initialized = false;
    char nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};
    char sharekey[32] = {0};
    unsigned char iv_decoded[16] = {0};
    unsigned char content_decoded[128] = {0};
    unsigned char content_decrypted[128] = {0};
    size_t iv_len = 0;
    size_t content_len = 0;
    *password_len = 0;
    password[0] = '\0';

    char* iv = NULL,* content = NULL;
    if (!cjson_parse_string_field(root, WEB_API_KEY_IV, &iv) ||
        !cjson_parse_string_field(root, WEB_API_KEY_CONTENT, &content)) {
        ESP_LOGE(TAG, "Failed to parse 'iv' or 'content'");
        goto cleanup;
    }

    if (webserver_get_client_nonce(sock_fd, nonce) != ESP_OK) {
        ESP_LOGE(TAG, "Client nonce not found");
        goto cleanup;
    }

    // Generate shared key via SHA-256
    if (sha256_get((unsigned char *)nonce, strlen(nonce), sharekey) != ESP_OK) {
        ESP_LOGE(TAG, "sha256_get failed");
        goto cleanup;
    }

    // Decode base64-encoded IV and content
    if (mbedtls_base64_decode(iv_decoded, sizeof(iv_decoded), &iv_len,
                              (unsigned char *)iv, strlen(iv)) != 0 ||
        iv_len != sizeof(iv_decoded) ||
        mbedtls_base64_decode(content_decoded, sizeof(content_decoded), &content_len,
                              (unsigned char *)content, strlen(content)) != 0 ||
        content_len == 0 || content_len >= sizeof(content_decrypted) ||
        (content_len % 16) != 0) {
        ESP_LOGE(TAG, "Base64 decode failed");
        goto cleanup;
    }

    // Decrypt using AES-CBC
    esp_aes_init(&ctx);
    aes_initialized = true;
    if (esp_aes_setkey(&ctx, (unsigned char *)sharekey, 256) != 0) {
        ESP_LOGE(TAG, "esp_aes_setkey failed");
        goto cleanup;
    }

    if (esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, content_len,
                          iv_decoded, content_decoded, content_decrypted) != 0) {
        ESP_LOGE(TAG, "esp_aes_crypt_cbc failed");
        goto cleanup;
    }
    // Parse decrypted JSON
    json_root = cJSON_Parse((char *)content_decrypted);
    if (!json_root) {
        ESP_LOGE(TAG, "JSON parse after decrypt failed");
        goto cleanup;
    }

    char *pswd_str = NULL;
    if (!cjson_parse_string_field(json_root, WEB_API_KEY_PASSWORD, &pswd_str) || !pswd_str) {
        goto cleanup;
    }
    size_t length = strlen(pswd_str);
    if (length == 0 || length >= 64) {
        ret = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }
    memcpy(password, pswd_str, length);
    password[length] = '\0';
    *password_len = length;
    ret = ESP_OK;

cleanup:
    if (json_root) cJSON_Delete(json_root);
    if (aes_initialized) esp_aes_free(&ctx);
    memset(nonce, 0, sizeof(nonce));
    memset(sharekey, 0, sizeof(sharekey));
    memset(iv_decoded, 0, sizeof(iv_decoded));
    memset(content_decoded, 0, sizeof(content_decoded));
    memset(content_decrypted, 0, sizeof(content_decrypted));
    if (ret != ESP_OK) {
        memset(password, 0, 64);
        *password_len = 0;
    }
    return ret;
}

/**
 * @brief Web API handler to set a new password via WebSocket.
 *
 * This function handles the decrypted password setting request from the client. The password is saved
 * to system configuration, used to update AP settings if needed, and a WebSocket response is returned
 * with the result.
 *
 * Special logic is included if this request is part of a "forgot password" workflow.
 *
 * @param data             JSON payload from WebSocket (already parsed).
 * @param now_resp_arg     Async response context, including socket FD and sequence ID.
 *
 * @return ESP_OK on success, or error code from processing steps.
 */
esp_err_t method_api_handle_set_password(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error_code = WEB_API_ERROR_FAIL;
    char password[64] = {0};
    size_t password_len = 0;
    webserver_config_t *config = NULL;
    webserver_client_node_t client = {0};
    bool has_client = webserver_client_find_copy(now_resp_arg->fd, &client);

    webserver_config_t *current_config = webserver_config_get_config();
    bool password_not_initialized = current_config && current_config->web_password[0] == '\0';
    bool password_change_allowed = password_not_initialized ||
                                   (has_client && client.auth) ||
                                   webserver_config_forget_password_is_authorized(now_resp_arg->fd);
    if (!password_change_allowed) {
        error_code = WEB_API_ERROR_AUTH;
        goto send_response;
    }

    if (parse_set_password(data, password, &password_len, now_resp_arg->fd) != ESP_OK) {
        goto send_response;
    }

    config = (webserver_config_t *)calloc(1, sizeof(webserver_config_t));
    if (config == NULL) {
        ESP_LOGE(TAG, "no mem");
        goto send_response;
    }

    if ((error_code = webserver_config_load_config_info_from_file(config)) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_load_config_info_from_file failed");
        goto send_response;
    }

    if (password_len > 0) {
        if (password_len > sizeof(password) - 1) {
            error_code = WEB_API_ERROR_PARAM_LEN;
            goto send_response;
        }
        if (login_session_revoke_all() != ESP_OK) {
            error_code = WEB_API_ERROR_SESSION_STORAGE;
            goto send_response;
        }
        strncpy(config->web_password, password, sizeof(config->web_password));
        if ((error_code = webserver_config_save_config_info_to_file(config)) != ESP_OK) {
            ESP_LOGE(TAG, "webserver_config_save_config_info_to_file failed");
            goto send_response;
        }
        provisioning_handoff_invalidate();
        if (has_client && client.auth) {
            webserver_client_set_auth_context(now_resp_arg->fd,
                                              true,
                                              client.is_ap ? WEB_AUTH_SOURCE_AP : WEB_AUTH_SOURCE_PASSWORD,
                                              NULL);
        }
        if (password_not_initialized && has_client && client.is_ap && client.auth) {
            provisioning_handoff_arm_from_initial_password(now_resp_arg->fd);
        }
    }

    error_code = WEB_API_ERROR_OK;
    // NetworkState *net = network_get_state();
    // if (net && !net->ap_params.password[0]) {
    //     network_wifi_ap_set_password((uint8_t *)password);
    // }
    webserver_close_other_wifi_and_eth_clients(now_resp_arg->fd);

send_response:
    memset(password, 0, sizeof(password));
    webserver_code_response_to_current_client(error_code, WEB_API_METHOD_SYS_SET_PASSWORD_RSP, now_resp_arg->seq);
    if (error_code == WEB_API_ERROR_OK) {
        webserver_config_forget_password_cancel_timer(now_resp_arg->fd);
    }
    if (config) {
        memset(config, 0, sizeof(*config));
        free(config);
    }
    return error_code;
}

/**
 * @brief Handle the "set login state" API request.
 *
 * This function verifies the correctness of the provided password from the incoming JSON data,
 * then sends a response back to the current client indicating whether the login was successful.
 *
 * @param data           Pointer to the cJSON object containing the request data (expected to contain login info).
 * @param now_resp_arg   Pointer to the asynchronous response argument structure, containing connection info and
 * sequence number.
 *
 * @return
 *      - ESP_OK if the response was successfully sent to the client.
 *      - An appropriate error code if verification or response sending fails.
 */
esp_err_t method_api_handle_set_login_state(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t auth_err = verify_password_is_correct(data, now_resp_arg->fd);
    int error_code = auth_err == ESP_OK ? WEB_API_ERROR_OK :
                     auth_err == ESP_ERR_TIMEOUT ? WEB_API_ERROR_SESSION_TOO_MANY_ATTEMPTS : WEB_API_ERROR_FAIL;
    cJSON *response = NULL;
    login_session_credential_t credential = {0};
    webserver_client_node_t client = {0};

    if (auth_err == ESP_OK) {
        response = cJSON_CreateObject();
        if (response) {
            cJSON_AddBoolToObject(response, "authenticated", true);
            char current_nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};
            if (webserver_get_client_nonce(now_resp_arg->fd,
                                           current_nonce) == ESP_OK) {
                cJSON_AddStringToObject(response, "nonce", current_nonce);
            }
            else {
                ESP_LOGE(TAG,
                         "Failed to include rotated nonce in login response: fd=%d",
                         now_resp_arg->fd);
            }
            bool session_issued = false;
            login_session_reason_t reason = LOGIN_SESSION_REASON_NONE;
            if (webserver_client_find_copy(now_resp_arg->fd, &client) && !client.is_ap &&
                login_session_issue(&credential, &reason) == ESP_OK) {
                session_issued = true;
                webserver_client_set_auth_context(now_resp_arg->fd,
                                                  true,
                                                  WEB_AUTH_SOURCE_PASSWORD,
                                                  credential.session_id);
                cJSON_AddStringToObject(response, "sessionId", credential.session_id);
                cJSON_AddStringToObject(response, "sessionToken", credential.session_token);
                cJSON_AddNumberToObject(response, "expiresAt", (double)credential.expires_at * 1000.0);
            }
            cJSON_AddBoolToObject(response, "sessionIssued", session_issued);
            if (!session_issued && !client.is_ap) {
                cJSON_AddStringToObject(response, "sessionIssueReason", login_session_reason_to_string(reason));
            }
        }
    }

    esp_err_t send_err = webserver_data_code_response_to_special_client_not_log(
        response,
        WEB_API_METHOD_SYS_SET_LOGIN_STATE_RSP,
        now_resp_arg->seq,
        error_code,
        now_resp_arg->fd);
    login_session_secure_zero(&credential, sizeof(credential));

    if (auth_err != ESP_OK && webserver_client_find_copy(now_resp_arg->fd, &client) && !client.is_ap) {
        webserver_login_authentication(now_resp_arg, false, 0);
    }
    return send_err;
}

/**
 * @brief Handle the reset device API call.
 *
 * This function initiates the device reset procedure.
 *
 * @param data          JSON data from client.
 * @param now_resp_arg  Async response argument containing client context.
 *
 * @return ESP_OK if the response was successfully sent, otherwise an error code.
 */
esp_err_t method_api_handle_reset_device(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret =
        webserver_code_response_to_current_client(WEB_API_ERROR_OK, WEB_API_METHOD_RESET_DEVICE_RSP, now_resp_arg->seq);
    webserver_config_restore_factory_settings();
    return ret;
}

/**
 * @brief Parses the IP whitelist configuration from a cJSON object.
 */
int webserver_config_parse_ip_white_list(cJSON *root, webserver_config_t *config)
{
    cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
    config->enabled = cJSON_IsTrue(enabled);
    cJSON *ip_list = cJSON_GetObjectItem(root, "ipWhitelist");
    if (!cJSON_IsArray(ip_list)) {
        ESP_LOGE(TAG, "IP whitelist data format error");
        return ESP_FAIL;
    }

    int ip_count = cJSON_GetArraySize(ip_list);
    if (ip_count > MAX_IP_COUNT) {
        ESP_LOGW(TAG, "The IP whitelist exceeds the maximum limit, Cut the first %d", MAX_IP_COUNT);
        ip_count = MAX_IP_COUNT;
    }

    config->ip_count = ip_count;
    for (int i = 0; i < ip_count; i++) {
        cJSON *ip_item = cJSON_GetArrayItem(ip_list, i);
        if (cJSON_IsString(ip_item) && ip_item->valuestring) {
            if (strlen(ip_item->valuestring) > MAX_IP_LEN - 1) {
                return WEB_API_ERROR_PARAM_LEN;
            }
            strncpy(config->ip_White_list[i], ip_item->valuestring, MAX_IP_LEN - 1);
            config->ip_White_list[i][MAX_IP_LEN - 1] = '\0';
        }
    }

    ESP_LOGI(TAG, "IP whitelist analysis completed, enabled status: %d, number: %d", config->enabled, config->ip_count);
    return ESP_OK;
}

/**
 * @brief Handle API request to set the IP whitelist configuration.
 */
esp_err_t method_api_handle_set_ip_white_list(cJSON *data, async_resp_arg *now_resp_arg)
{
    webserver_config_t config = {0};
    webserver_config_t temp_config = {0};
    esp_err_t ret = webserver_config_parse_ip_white_list(data, &temp_config);
    if (ret == ESP_OK) {
        ret = webserver_config_load_config_info_from_file(&config);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "webserver_config_load_config_info_from_file fail");
        }

        config.enabled = temp_config.enabled;
        memset(config.ip_White_list, 0, sizeof(config.ip_White_list));
        config.ip_count = temp_config.ip_count;
        for (int i = 0; i < temp_config.ip_count && i < MAX_IP_COUNT; i++) {
            strncpy(config.ip_White_list[i], temp_config.ip_White_list[i], MAX_IP_LEN - 1);
            config.ip_White_list[i][MAX_IP_LEN - 1] = '\0'; /* 确保字符串终结 */
            ESP_LOGI(TAG, "IP[%d]: %s", i, config.ip_White_list[i]);
        }
        ret = webserver_config_save_config_info_to_file(&config);
        int error_code = WEB_API_ERROR_OK;
        if (ESP_OK != ret) {
            error_code = WEB_API_ERROR_WHITE_LIST_IP_ALREADY_EXIST;
            ESP_LOGE(TAG, "saveConfig fail or white list ip already exist");
        }
        webserver_code_response_to_current_client(error_code, WEB_API_METHOD_SECU_SET_IP_WHITE_LIST_RSP,
                                                   now_resp_arg->seq);
        webserver_whitelist_notify();
        return ret;
    }
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SECU_SET_IP_WHITE_LIST_RSP, now_resp_arg->seq);
    return ret;
}

/**
 * @brief Parses theme mode configuration from a cJSON object.
 */
static int webserver_config_parse_theme_mode(cJSON *root, theme_mode_t *theme_mode)
{
    if (root == NULL || theme_mode == NULL) {
        return ESP_FAIL;
    }

    cJSON *mode = cJSON_GetObjectItem(root, "currentMode");
    cJSON *follow = cJSON_GetObjectItem(root, "followSystem");

    if (mode && cJSON_IsString(mode)) {
        strncpy(theme_mode->current_mode, mode->valuestring, sizeof(theme_mode->current_mode) - 1);
        theme_mode->current_mode[sizeof(theme_mode->current_mode) - 1] = '\0';
    }
    if (follow && cJSON_IsBool(follow)) {
        theme_mode->follow_system = cJSON_IsTrue(follow);
    }
    return ESP_OK;
}

/**
 * @brief Handle API request to set the device theme mode.
 */
esp_err_t method_api_handle_set_theme_mode(cJSON *data, async_resp_arg *now_resp_arg)
{
    device_info_t device_info;
    esp_err_t ret = ESP_FAIL;
    if (webserver_config_load_device_info_from_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_load_config_info_from_file fail");
        goto send_response;
    }
    if (webserver_config_parse_theme_mode(data, &device_info.theme_mode) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_parse_theme_mode fail");
        goto send_response;
    }
    if (webserver_config_save_device_info_to_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_save_device_info_to_file fail");
        goto send_response;
    }
    ret = ESP_OK;
    webserver_theme_mode_notify();
send_response:
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SET_THEME_MODE_RSP, now_resp_arg->seq);
    return ret;
}

/**
 * @brief Handle API request to set the device language.
 */
esp_err_t method_api_handle_set_language(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret = ESP_FAIL;
    device_info_t device_info;
    char *language;
    if (!cjson_parse_string_field(data, "language", &language)) {
        ESP_LOGE(TAG, "cjson_parse_string_field fail");
        goto send_response;
    }
    if (webserver_config_load_device_info_from_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_load_config_info_from_file fail");
        goto send_response;
    }
    strncpy(device_info.language, language, sizeof(device_info.language) - 1);
    if (webserver_config_save_device_info_to_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_save_device_info_to_file fail");
        goto send_response;
    }

    ret = ESP_OK;
    webserver_device_info_notify(UPDATE_LANGUAGE);
send_response:
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SET_LANGUAGE_RSP, now_resp_arg->seq);
    return ret;
}


/**
 * @brief Handle API request to set the device screen lock idle time.
 */
esp_err_t method_api_handle_set_screen_lock(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret = ESP_FAIL;
    device_info_t device_info;
    cJSON *json_idle_time = cJSON_GetObjectItemCaseSensitive(data, "idleTime");
    if (!json_idle_time)
    {
        ESP_LOGE(TAG, "parse idleTime fail");
        goto send_response;
    }
    int idle_time = (int)cJSON_GetNumberValue(json_idle_time);
    ESP_LOGI(TAG, "idleTime value: %d", idle_time);
    if (idle_time < 0) {
        ESP_LOGE(TAG, "idleTime out of range, must be >= 0");
        goto send_response;
    }
    if (webserver_config_load_device_info_from_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_load_config_info_from_file fail");
        goto send_response;
    }
    device_info.idle_time =(uint32_t)idle_time;
    if (webserver_config_save_device_info_to_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_save_device_info_to_file fail");
        goto send_response;
    }
    ret = screen_set_lock(idle_time, device_info.brightness);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "screen_set_lock fail");
        goto send_response;
    }
    webserver_device_info_notify(UPDATE_IDLE_TIME);
    webserver_code_response_to_all_client(ret, WEB_API_METHOD_SET_SCREEN_LOCK_RSP, now_resp_arg->seq);
    return ret;
send_response:
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SET_SCREEN_LOCK_RSP, now_resp_arg->seq);
    return ret;
}

esp_err_t method_api_handle_set_brightness(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret = ESP_FAIL;
    device_info_t device_info;
    cJSON *json_set_brightness = cJSON_GetObjectItemCaseSensitive(data, "brightness");
    if (!json_set_brightness)
    {
        ESP_LOGE(TAG, "parse brightness fail");
        goto send_response;
    }
    int brightness = (int)cJSON_GetNumberValue(json_set_brightness);
    if (brightness < 0 || brightness > 100) {
        ESP_LOGE(TAG, "out of range brightness");
        goto send_response;
    }
    if (webserver_config_load_device_info_from_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_load_config_info_from_file fail");
        goto send_response;
    }
    device_info.brightness = brightness;
    if (webserver_config_save_device_info_to_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_save_device_info_to_file fail");
        goto send_response;
    }
    night_mode_notify_manual_brightness();
    bsp_lcd_br_set(night_mode_get_effective_brightness(brightness));
    screen_lock_reset_timer();
    ret = ESP_OK;
    ESP_LOGI(TAG, "brightness value: %d", brightness);
    webserver_device_info_notify(UPDATE_BRIGHTNESS);
    webserver_code_response_to_all_client(ret, WEB_API_METHOD_SET_BRIGHTNESS_RSP, now_resp_arg->seq);
    return ret;
send_response:
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SET_BRIGHTNESS_RSP, now_resp_arg->seq);
    return ret;
}

esp_err_t method_api_handle_set_night_mode(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret = ESP_FAIL;
    cJSON *json_enable = cJSON_GetObjectItemCaseSensitive(data, "enable");
    if (!json_enable)
    {
        ESP_LOGE(TAG, "parse enable fail");
        goto send_response;
    }
    cJSON *json_brightness = cJSON_GetObjectItemCaseSensitive(data, "brightness");
    if (!json_brightness)
    {
        ESP_LOGE(TAG, "parse brightness fail");
        goto send_response;
    }
    char *from_str = NULL;
    char *to_str = NULL;
    if (!cjson_parse_string_field(data,"from",&from_str))
    {
        ESP_LOGE(TAG, "parse from fail");
        goto send_response;
    }
    if (!cjson_parse_string_field(data,"to",&to_str))
    {
        ESP_LOGE(TAG, "parse to fail");
        goto send_response;
    }
    bool enable = cJSON_IsTrue(json_enable);
    int brightness = (int)cJSON_GetNumberValue(json_brightness);
    if (brightness < 0 || brightness > 100) {
        ESP_LOGE(TAG, "out of range brightness");
        goto send_response;
    }
    if (strlen(from_str) > NIGHT_MODE_FROM_LEN || strlen(to_str) > NIGHT_MODE_TO_LEN) {
        ESP_LOGE(TAG, "from or to too long");
        goto send_response;
    }
    ret = night_mode_api_set_config(enable,(uint8_t)brightness, from_str, to_str);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "night_mode_api_set_config fail");
        goto send_response;
    }
    ESP_LOGI(TAG, "night mode: enable=%d, brightness=%d, from=%s, to=%s", enable, brightness, from_str, to_str);
    webserver_device_info_notify(UPDATE_NIGHT_MODE);
    webserver_code_response_to_all_client(ret, WEB_API_METHOD_SET_NIGHT_MODE_RSP, now_resp_arg->seq);
    return ret;
send_response:
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SET_NIGHT_MODE_RSP, now_resp_arg->seq);
    return ret;
}

/**
 * @brief Handle API request to set the device product name.
 */
esp_err_t method_api_handle_set_name(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret = ESP_FAIL;
    device_info_t device_info;
    char *product_name;
    if (!cjson_parse_string_field(data, "name", &product_name)) {
        ESP_LOGE(TAG, "cjson_parse_string_field name fail");
        goto send_response;
    }
    if (strlen(product_name) > sizeof(device_info.product_name) - 1) {
        ESP_LOGE(TAG, "product_name too long");
        ret = WEB_API_ERROR_PARAM_LEN;
        goto send_response;
    }
    if (webserver_config_load_device_info_from_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_load_config_info_from_file fail");
        goto send_response;
    }
    snprintf(device_info.product_name, sizeof(device_info.product_name), "%s", product_name);
    // printf("set new name: %s\n", device_info.product_name);
    if (webserver_config_save_device_info_to_file(&device_info) != ESP_OK) {
        ESP_LOGE(TAG, "webserver_config_save_device_info_to_file fail");
        goto send_response;
    }
    // mqtt_sync_url();
    ret = webserver_code_response_to_current_client(WEB_API_ERROR_OK, WEB_API_METHOD_SET_NAME_RSP, now_resp_arg->seq);
    webserver_device_info_notify(UPDATE_NAME);
    // mdns_server_update_server_txt();
    // webserver_cur_mdns_scan_result_notify();
    return ret;

send_response:
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SET_NAME_RSP, now_resp_arg->seq);
    return ret;
}

/**
 * @brief Handle API request to reboot device components.
 */
esp_err_t method_api_handle_reboot_device(cJSON *data, async_resp_arg *now_resp_arg)
{
    // if (firmware_manage_check_ota_is_doing()) {
    //     webserver_code_response_to_current_client(WEB_API_ERROR_OTA_IS_DOING, WEB_API_METHOD_REBOOT_DEVICE_RSP,
    //                                               now_resp_arg->seq);
    //     return ESP_FAIL;
    // }
    char *target;
    if (!cjson_parse_string_field(data, "target", &target)) {
        webserver_code_response_to_current_client(WEB_API_ERROR_FAIL, WEB_API_METHOD_REBOOT_DEVICE_RSP,
                                                  now_resp_arg->seq);
        return ESP_FAIL;
    }

    if (strcmp(target, ESP32_REBOOT) == 0) {
        webserver_code_response_to_current_client(WEB_API_ERROR_OK, WEB_API_METHOD_REBOOT_DEVICE_RSP,
                                                  now_resp_arg->seq);
        esp32_app_restart();
    }
    // else if (strcmp(target, WIRELESS_CONTROLLER_REBOOT) == 0) {
    //     wireless_controller_reboot();
    //     webserver_code_response_to_current_client(WEB_API_ERROR_OK, WEB_API_METHOD_REBOOT_DEVICE_RSP,
    //                                               now_resp_arg->seq);
    //     return ESP_OK;
    // }
    // else if (strcmp(target, ALL_REBOOT) == 0) {
    //     webserver_code_response_to_current_client(WEB_API_ERROR_OK, WEB_API_METHOD_REBOOT_DEVICE_RSP,
    //                                               now_resp_arg->seq);
    //     wireless_controller_reboot();
    //     esp32_app_restart();
    // }
    else {
        webserver_code_response_to_current_client(WEB_API_ERROR_FAIL, WEB_API_METHOD_REBOOT_DEVICE_RSP,
                                                  now_resp_arg->seq);
        return ESP_FAIL;
    }
    return ESP_FAIL;
}

void esp32_app_restart()
{
    screen_state_request_transition(SCREEN_PAGE_RESTARTING);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    // tcp_disconnect_current_session("NEED RESTART");
    webserver_force_free_all_clients();
    // notification_manage_esp32_reboot_notify_tigger();
    esp_restart();
}
