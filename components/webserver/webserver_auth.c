#include <esp_log.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <cJSON.h>
#include "webserver_config.h"
#include "sys_time.h"
#include "cjson_helpers.h"
#include "webserver.h"
#include "sys_time_api.h"
#include "webserver_config_api.h"
#include "webserver_client_node.h"
#include "mdns_api.h"

static const char *TAG = "webserver_auth";

#define SHA256_DIGEST_LEN 32
#define SHA256_STR_LEN (SHA256_DIGEST_LEN * 2 + 1)

static const char *WEB_AUTH_USERNAME = "admin";

/**
 * @brief Converts binary data to a hexadecimal string.
 *
 * This function takes a buffer of binary data and converts it into a
 * null-terminated hexadecimal string representation. Each byte is represented
 * by two lowercase hexadecimal characters.
 *
 * @param input   Pointer to the binary input data.
 * @param len     Length of the input data in bytes.
 * @param output  Pointer to the output buffer. Must be at least (len * 2 + 1) bytes long
 *                to accommodate the resulting hex string and null terminator.
 */
static void webserver_bytes_to_hex_str(const uint8_t *input, size_t len, char *output)
{
    for (size_t i = 0; i < len; ++i) {
        sprintf(output + i * 2, "%02x", input[i]);
    }
    output[len * 2] = '\0';
}

/**
 * @brief Generate a SHA-256-based HTTP Digest Authentication response string.
 *
 * This function constructs a digest authentication response using the HTTP Digest Access
 * Authentication model, but with SHA-256 as the hashing algorithm. It combines the provided
 * credentials and parameters (username, password, realm, nonce, cnonce, and qop) to generate
 * a valid response hash.
 *
 * The hashing follows this structure:
 *   HA1 = SHA256(username:realm:password)
 *   HA2 = SHA256(method:uri)        // In this case, hardcoded as "dummy_method:dummy_uri"
 *   Response = SHA256(HA1:nonce:cnonce:qop:HA2)
 *
 * @param username            User's name
 * @param realm               Authentication realm
 * @param password            User's password
 * @param nonce               Server-supplied nonce
 * @param cnonce              Client-supplied nonce
 * @param qop                 Quality of protection (e.g., "auth")
 * @param response_str_out    Output buffer to hold the resulting hex-encoded SHA-256 response string.
 *                            Must be at least SHA256_STR_LEN bytes in size.
 *
 * @return
 *         - ESP_OK on success
 *         - ESP_FAIL on failure (e.g., invalid input or memory allocation error)
 */
static esp_err_t webserver_generate_auth_str(const char *username, const char *realm, const char *password,
                                             const char *nonce, const char *cnonce, const char *qop,
                                             char *response_str_out)
{
    if (!username || !realm || !password || !nonce || !cnonce || !qop || !response_str_out) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_FAIL;
    }
    char ha1_digest[SHA256_DIGEST_LEN] = {0};
    char ha1_str[SHA256_STR_LEN] = {0};
    char ha2_digest[SHA256_DIGEST_LEN] = {0};
    char ha2_str[SHA256_STR_LEN] = {0};
    char response_digest[SHA256_DIGEST_LEN] = {0};
    char response_str[SHA256_STR_LEN] = {0};
    esp_err_t result = ESP_FAIL;

    const size_t buf_size = 300;
    char *buf = (char *)calloc(1, buf_size);

    if (!buf) {
        ESP_LOGE(TAG, "No heap for buffer");
        return ESP_FAIL;
    }

    int length = snprintf(buf, buf_size, "%s:%s:%s", username, realm, password);
    if (length < 0 || (size_t)length >= buf_size ||
        sha256_get((uint8_t *)buf, (uint32_t)length, ha1_digest) != ESP_OK) goto cleanup;
    webserver_bytes_to_hex_str((uint8_t *)ha1_digest, SHA256_DIGEST_LEN, ha1_str);

    length = snprintf(buf, buf_size, "dummy_method:dummy_uri");
    if (length < 0 || (size_t)length >= buf_size ||
        sha256_get((uint8_t *)buf, (uint32_t)length, ha2_digest) != ESP_OK) goto cleanup;
    webserver_bytes_to_hex_str((uint8_t *)ha2_digest, SHA256_DIGEST_LEN, ha2_str);

    length = snprintf(buf, buf_size, "%s:%s:%s:%s:%s", ha1_str, nonce, cnonce, qop, ha2_str);
    if (length < 0 || (size_t)length >= buf_size ||
        sha256_get((uint8_t *)buf, (uint32_t)length, response_digest) != ESP_OK) goto cleanup;
    webserver_bytes_to_hex_str((uint8_t *)response_digest, SHA256_DIGEST_LEN, response_str);

    memcpy(response_str_out, response_str, SHA256_STR_LEN);
    result = ESP_OK;

cleanup:
    memset(buf, 0, buf_size);
    free(buf);
    memset(ha1_digest, 0, sizeof(ha1_digest));
    memset(ha1_str, 0, sizeof(ha1_str));
    memset(ha2_digest, 0, sizeof(ha2_digest));
    memset(ha2_str, 0, sizeof(ha2_str));
    memset(response_digest, 0, sizeof(response_digest));
    memset(response_str, 0, sizeof(response_str));
    return result;
}

/**
 * @brief Checks whether the given client IP address is authenticated.
 *
 * This function verifies if the specified IP address is present in the list of
 * authenticated client IPs (`webserver_client_ip_authed[]`). If found, it means the client
 * has previously passed authentication.
 *
 * @param ip  Pointer to a 32-bit integer representing the client IP address (in host byte order).
 *            Must not be NULL or zero.
 *
 * @return
 *         - true if the IP is found in the authenticated IP list
 *         - false if the IP is NULL, invalid (0), or not authenticated
 */
static void webserver_auth_get_realm(char realm[32])
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(realm, 32, "Open-Nextion-%02X%02X", mac[4], mac[5]);
}

esp_err_t webserver_auth_rotate_nonce(int sock_fd, char *nonce_out, size_t nonce_out_size)
{
    static const char hex[] = "0123456789abcdef";
    uint8_t random[WEB_AUTH_NONCE_HEX_LEN / 2];
    char nonce[WEB_AUTH_NONCE_HEX_LEN + 1];
    esp_fill_random(random, sizeof(random));
    for (size_t i = 0; i < sizeof(random); i++) {
        nonce[i * 2] = hex[random[i] >> 4];
        nonce[i * 2 + 1] = hex[random[i] & 0x0f];
    }
    nonce[WEB_AUTH_NONCE_HEX_LEN] = '\0';
    memset(random, 0, sizeof(random));
    if (!webserver_client_update_nonce(sock_fd, nonce)) {
        memset(nonce, 0, sizeof(nonce));
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG,
             "Rotated client nonce: fd=%d tag=%.8s",
             sock_fd,
             nonce);
    if (nonce_out && nonce_out_size > 0) strlcpy(nonce_out, nonce, nonce_out_size);
    memset(nonce, 0, sizeof(nonce));
    return ESP_OK;
}

static bool webserver_auth_is_lower_hex(const char *value, size_t expected_length)
{
    if (!value || strnlen(value, expected_length + 1) != expected_length) return false;
    for (size_t i = 0; i < expected_length; i++) {
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
    }
    return true;
}

static bool webserver_auth_is_client_nonce(const char *value)
{
    if (!value) return false;
    size_t length = strnlen(value, 65);
    if (length < 8 || length > 64) return false;
    for (size_t i = 0; i < length; i++) {
        char c = value[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

static bool webserver_auth_constant_time_string_equal(const char *a, const char *b, size_t length)
{
    unsigned char difference = 0;
    for (size_t i = 0; i < length; i++) difference |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return difference == 0;
}

/**
 * @brief Internal implementation for verifying login password using digest auth.
 *
 * This function extracts the fields `username`, `realm`, `nonce`, `cnonce`, `qop`,
 * `response`, and `algorithm` from the given JSON root object. It then loads the stored
 * configuration (including the password), regenerates the expected response string using
 * `webserver_generate_auth_str`, and compares it with the client-provided `response`.
 *
 * If the credentials are valid, an `WEB_SERVER_EVENT_AUTH_OK` event is posted.
 *
 * Note:
 * - The digest is calculated using SHA-256.
 * - Currently, the "algorithm" field is parsed but not used.
 * - Hardcoded "dummy_method:dummy_uri" is used in HA2 computation.
 *
 * @param root Parsed cJSON object containing all required digest auth fields.
 * @param sock_fd Socket file descriptor identifying the client to update..
 * @return
 *         - ESP_OK if credentials are valid
 *         - ESP_FAIL on parse error, config load error, or auth mismatch
 */
esp_err_t verify_password_is_correct(cJSON *root, int sock_fd)
{
    if (!webserver_client_auth_attempt_allowed(sock_fd)) return ESP_ERR_TIMEOUT;

    char *username = NULL;
    char *realm = NULL;
    char *nonce = NULL;
    char *cnonce = NULL;
    char *qop = NULL;
    char *response = NULL;
    char *algorithm = NULL;
    char result[SHA256_STR_LEN] = {0};
    esp_err_t auth_result = ESP_FAIL;

    if (!root ||
        !cjson_parse_string_field(root, WEB_API_KEY_NAME, &username) ||
        !cjson_parse_string_field(root, WEB_API_KEY_REALM, &realm) ||
        !cjson_parse_string_field(root, WEB_API_KEY_NONCE, &nonce) ||
        !cjson_parse_string_field(root, WEB_API_KEY_C_NONCE, &cnonce) ||
        !cjson_parse_string_field(root, WEB_API_KEY_QOP, &qop) ||
        !cjson_parse_string_field(root, WEB_API_KEY_RSP, &response) ||
        !cjson_parse_string_field(root, WEB_API_KEY_C_ALGORITHM, &algorithm)) {
        goto authentication_failed;
    }

    webserver_client_node_t client = {0};
    char expected_realm[32] = {0};
    webserver_auth_get_realm(expected_realm);
    if (!webserver_client_find_copy(sock_fd, &client) ||
        strcmp(username, WEB_AUTH_USERNAME) != 0 ||
        strcmp(realm, expected_realm) != 0 ||
        strcmp(qop, "auth") != 0 ||
        strcmp(algorithm, "sha256") != 0 ||
        !webserver_auth_is_lower_hex(nonce, WEB_AUTH_NONCE_HEX_LEN) ||
        !webserver_auth_is_lower_hex(response, SHA256_STR_LEN - 1) ||
        !webserver_auth_is_client_nonce(cnonce) ||
        strcmp(nonce, client.nonce) != 0) {
        goto authentication_failed;
    }

    webserver_config_t *config = webserver_config_get_config();
    if (!config || webserver_generate_auth_str(username, realm, config->web_password,
                                               nonce, cnonce, qop, result) != ESP_OK) {
        ESP_LOGW(TAG, "webserver_generate_auth_str error");
        goto authentication_failed;
    }

    if (webserver_auth_constant_time_string_equal(result, response, SHA256_STR_LEN - 1)) {
        auth_result = ESP_OK;
        webserver_client_set_auth_context(sock_fd, true, WEB_AUTH_SOURCE_PASSWORD, NULL);
        esp_event_post(WEB_SERVER_EVENT, WEB_SERVER_EVENT_AUTH_OK, NULL, 0, portMAX_DELAY);
        goto cleanup;
    }

authentication_failed:
    webserver_client_record_auth_failure(sock_fd);

cleanup:
    webserver_auth_rotate_nonce(sock_fd, NULL, 0);
    memset(result, 0, sizeof(result));
    return auth_result;
}

/**
 * @brief Creates and sends a JSON notification to the current client indicating
 *        whether it is connected to the AP network.
 *
 * The notification JSON has a single boolean field "connectWithAp" which reflects
 * the connection state.
 *
 * Uses cJSON to create the JSON object and sends it via `webserver_data_response_to_current_client`.
 *
 * @param is_ap_connect  Flag indicating AP connection status (true/non-zero = connected).
 *
 * @return
 *         - ESP_OK if notification was successfully sent
 *         - ESP_FAIL if JSON object creation failed
 */
static int webserver_network_connection_status_response(uint8_t is_ap_connect)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        ESP_LOGE(TAG, "create data object failed");
        return ESP_FAIL;
    }
    cJSON_AddBoolToObject(data, "connectWithAp", is_ap_connect ? true : false);
    webserver_data_response_to_current_client(data, WEB_API_METHOD_NET_CONN_STATUS_NOTIFY, 0);
    return ESP_OK;
}

/**
 * @brief Internal implementation of login logic and authentication handshake.
 *
 * This function is called when a client connects. It checks:
 *  - Whether the client IP address is authenticated (`webserver_client_ip_is_authed`)
 *  - Whether the client is from the AP subnet (in which case authentication may be skipped)
 *  - Whether the password is initialized
 *  - Whether the maximum connection limit is reached (websocket webserver_node_clients)
 *
 * Depending on these conditions:
 *  - If the password is not initialized, it sends `passwordInitRequired: true`
 *  - If the connection limit is hit, it sends `connectionLimitReached: true`
 *  - If password is required, it generates a SHA-256 digest challenge
 *
 * A cryptographically random 256-bit hexadecimal value is used as the `nonce`, and the challenge is sent to the client
 * via a JSON notification using `webserver_data_response_to_current_client()`.
 *
 * Also notifies network connection status and triggers relevant events.
 *
 * Notes:
 *  - Uses device MAC address to construct the authentication realm
 *  - Memory for config is allocated and freed in this function
 *
 * @param resp_arg  Pointer to async response argument for client session
 *
 * @return
 *         - ESP_OK if notification sent successfully
 *         - ESP_FAIL on memory allocation failure or config load error
 */
esp_err_t webserver_login_authentication(async_resp_arg *resp_arg, bool is_ap_subnet, uint32_t ip_addr)
{
    (void)ip_addr;
    bool passwordRequired = !is_ap_subnet;
    bool passwordInitRequired = false;
    bool connectionLimit = websocket_client_is_limit();

    webserver_config_t *config = webserver_config_get_config();
    if (strlen(config->web_password) == 0) {
        passwordInitRequired = true;
        ESP_LOGI(TAG, "need set init password");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) return ESP_ERR_NO_MEM;
    if (passwordInitRequired) {
        cJSON_AddBoolToObject(data, "passwordInitRequired", true);
    }
    else if (connectionLimit) {
        cJSON_AddBoolToObject(data, "connectionLimitReached", true);
    }
    else {
        cJSON_AddBoolToObject(data, "passwordRequired", passwordRequired);
        if (passwordRequired) {
            char realm_str[32] = {0};
            webserver_auth_get_realm(realm_str);

            cJSON_AddStringToObject(data, "realm", realm_str);
            cJSON_AddStringToObject(data, "algorithm", "sha256");
            cJSON_AddStringToObject(data, "qop", "auth");
            cJSON_AddBoolToObject(data, "sessionResumeSupported", true);
        }
    }
    char nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};
    if (webserver_auth_rotate_nonce(resp_arg->fd, nonce, sizeof(nonce)) != ESP_OK) {
        cJSON_Delete(data);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(data, "nonce", nonce);

    ESP_LOGI(TAG, "webserver_login_authentication: fd=%d, is_ap=%d, pwdInit=%d",
             resp_arg->fd, is_ap_subnet, passwordInitRequired);

    esp_err_t send_err = webserver_data_response_to_current_client(data, WEB_API_METHOD_SYS_AUTH_NTY, 0);
    ESP_LOGI(TAG, "webserver_data_response_to_current_client returned: %d", send_err);

    webserver_network_connection_status_response(is_ap_subnet);
    
    webserver_theme_mode_notify();
    webserver_language_notify();
    webserver_cur_mdns_scan_result_notify();
    if (is_ap_subnet) {
        esp_event_post(WEB_SERVER_EVENT, WEB_SERVER_EVENT_SKIP_AUTH, NULL, 0, portMAX_DELAY);
    }
    return ESP_OK;
}
