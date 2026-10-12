#include "provisioning_handoff.h"

#include <stdbool.h>
#include <string.h>

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"

#include "login_session.h"
#include "webserver.h"
#include "webserver_auth.h"
#include "webserver_client_node.h"

#define HANDOFF_TICKET_BYTES 32
#define HANDOFF_DIGEST_BYTES 32
#define HANDOFF_ARM_TTL_US (10LL * 60LL * 1000LL * 1000LL)
#define HANDOFF_TICKET_TTL_US (2LL * 60LL * 1000LL * 1000LL)

typedef struct {
    bool armed;
    in_addr_t ap_peer_ip;
    int64_t arm_expires_at_us;
    bool ticket_valid;
    uint8_t ticket_digest[HANDOFF_DIGEST_BYTES];
    int64_t ticket_expires_at_us;
} provisioning_handoff_state_t;

static const char *TAG = "provision_handoff";
static provisioning_handoff_state_t s_handoff;
static portMUX_TYPE s_handoff_lock = portMUX_INITIALIZER_UNLOCKED;

static void bytes_to_hex(const uint8_t *bytes, size_t length, char *output)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        output[i * 2] = hex[bytes[i] >> 4];
        output[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    output[length * 2] = '\0';
}

static bool is_lower_hex(const char *value, size_t expected_length)
{
    if (!value || strnlen(value, expected_length + 1) != expected_length) return false;
    for (size_t i = 0; i < expected_length; ++i) {
        if (!((value[i] >= '0' && value[i] <= '9') ||
              (value[i] >= 'a' && value[i] <= 'f'))) return false;
    }
    return true;
}

static bool constant_time_equal(const uint8_t *left, const uint8_t *right, size_t length)
{
    uint8_t difference = 0;
    for (size_t i = 0; i < length; ++i) difference |= left[i] ^ right[i];
    return difference == 0;
}

void provisioning_handoff_invalidate(void)
{
    portENTER_CRITICAL(&s_handoff_lock);
    memset(&s_handoff, 0, sizeof(s_handoff));
    portEXIT_CRITICAL(&s_handoff_lock);
}

void provisioning_handoff_arm_from_initial_password(int socket_fd)
{
    webserver_client_node_t client = {0};
    in_addr_t peer_ip = 0;
    if (!webserver_client_find_copy(socket_fd, &client) || !client.is_ap || !client.auth ||
        !webserver_client_ip_is_ap_subnet(socket_fd, &peer_ip) || peer_ip == 0) {
        ESP_LOGW(TAG, "Refusing to arm handoff for fd=%d", socket_fd);
        return;
    }

    portENTER_CRITICAL(&s_handoff_lock);
    memset(&s_handoff, 0, sizeof(s_handoff));
    s_handoff.armed = true;
    s_handoff.ap_peer_ip = peer_ip;
    s_handoff.arm_expires_at_us = esp_timer_get_time() + HANDOFF_ARM_TTL_US;
    portEXIT_CRITICAL(&s_handoff_lock);
    ESP_LOGI(TAG, "Initial provisioning handoff armed for fd=%d", socket_fd);
}

esp_err_t provisioning_handoff_issue(int socket_fd,
                                     char ticket_out[PROVISIONING_HANDOFF_TICKET_HEX_LEN + 1])
{
    if (!ticket_out) return ESP_ERR_INVALID_ARG;
    ticket_out[0] = '\0';

    webserver_client_node_t client = {0};
    in_addr_t peer_ip = 0;
    if (!webserver_client_find_copy(socket_fd, &client) || !client.is_ap || !client.auth ||
        !webserver_client_ip_is_ap_subnet(socket_fd, &peer_ip)) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t now = esp_timer_get_time();
    bool eligible = false;
    portENTER_CRITICAL(&s_handoff_lock);
    if (s_handoff.armed && now <= s_handoff.arm_expires_at_us &&
        peer_ip == s_handoff.ap_peer_ip) {
        s_handoff.armed = false;
        eligible = true;
    }
    portEXIT_CRITICAL(&s_handoff_lock);
    if (!eligible) return ESP_ERR_INVALID_STATE;

    uint8_t random_ticket[HANDOFF_TICKET_BYTES];
    uint8_t digest[HANDOFF_DIGEST_BYTES];
    esp_fill_random(random_ticket, sizeof(random_ticket));
    bytes_to_hex(random_ticket, sizeof(random_ticket), ticket_out);
    memset(random_ticket, 0, sizeof(random_ticket));
    if (sha256_get((const uint8_t *)ticket_out,
                   PROVISIONING_HANDOFF_TICKET_HEX_LEN,
                   (char *)digest) != ESP_OK) {
        memset(ticket_out, 0, PROVISIONING_HANDOFF_TICKET_HEX_LEN + 1);
        memset(digest, 0, sizeof(digest));
        return ESP_FAIL;
    }

    portENTER_CRITICAL(&s_handoff_lock);
    memcpy(s_handoff.ticket_digest, digest, sizeof(digest));
    s_handoff.ticket_expires_at_us = esp_timer_get_time() + HANDOFF_TICKET_TTL_US;
    s_handoff.ticket_valid = true;
    portEXIT_CRITICAL(&s_handoff_lock);
    memset(digest, 0, sizeof(digest));
    ESP_LOGI(TAG, "Issued one-time provisioning handoff for fd=%d", socket_fd);
    return ESP_OK;
}

static bool consume_ticket(const char *ticket)
{
    if (!is_lower_hex(ticket, PROVISIONING_HANDOFF_TICKET_HEX_LEN)) return false;

    uint8_t digest[HANDOFF_DIGEST_BYTES];
    if (sha256_get((const uint8_t *)ticket,
                   PROVISIONING_HANDOFF_TICKET_HEX_LEN,
                   (char *)digest) != ESP_OK) return false;

    bool valid = false;
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_handoff_lock);
    if (s_handoff.ticket_valid && now <= s_handoff.ticket_expires_at_us &&
        constant_time_equal(digest, s_handoff.ticket_digest, sizeof(digest))) {
        valid = true;
    }
    if (valid || (s_handoff.ticket_valid && now > s_handoff.ticket_expires_at_us)) {
        s_handoff.ticket_valid = false;
        memset(s_handoff.ticket_digest, 0, sizeof(s_handoff.ticket_digest));
        s_handoff.ticket_expires_at_us = 0;
    }
    portEXIT_CRITICAL(&s_handoff_lock);
    memset(digest, 0, sizeof(digest));
    return valid;
}

esp_err_t method_api_handle_exchange_provisioning_handoff(cJSON *data,
                                                           async_resp_arg *now_resp_arg)
{
    const cJSON *ticket_item = cJSON_GetObjectItemCaseSensitive(data, "ticket");
    webserver_client_node_t client = {0};
    int error = WEB_API_ERROR_HANDOFF_INVALID;
    cJSON *response = cJSON_CreateObject();
    login_session_credential_t credential = {0};

    if (!response) return ESP_ERR_NO_MEM;
    if (!webserver_client_find_copy(now_resp_arg->fd, &client) || client.is_ap || client.auth ||
        !cJSON_IsString(ticket_item) || !ticket_item->valuestring ||
        !webserver_client_auth_attempt_allowed(now_resp_arg->fd) ||
        !consume_ticket(ticket_item->valuestring)) {
        webserver_client_record_auth_failure(now_resp_arg->fd);
        cJSON_AddBoolToObject(response, "authenticated", false);
        goto send_response;
    }

    char rotated_nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};
    if (webserver_auth_rotate_nonce(now_resp_arg->fd, rotated_nonce, sizeof(rotated_nonce)) != ESP_OK) {
        cJSON_AddBoolToObject(response, "authenticated", false);
        error = WEB_API_ERROR_FAIL;
        goto send_response;
    }

    login_session_reason_t reason = LOGIN_SESSION_REASON_NONE;
    bool session_issued = login_session_issue(&credential, &reason) == ESP_OK;
    webserver_client_set_auth_context(now_resp_arg->fd,
                                      true,
                                      session_issued ? WEB_AUTH_SOURCE_SESSION : WEB_AUTH_SOURCE_PROVISIONING,
                                      session_issued ? credential.session_id : NULL);
    webserver_client_reset_auth_failures(now_resp_arg->fd);
    cJSON_AddBoolToObject(response, "authenticated", true);
    cJSON_AddStringToObject(response, "nonce", rotated_nonce);
    cJSON_AddBoolToObject(response, "sessionIssued", session_issued);
    if (session_issued) {
        cJSON_AddStringToObject(response, "sessionId", credential.session_id);
        cJSON_AddStringToObject(response, "sessionToken", credential.session_token);
        cJSON_AddNumberToObject(response, "expiresAt", (double)credential.expires_at * 1000.0);
    }
    else {
        cJSON_AddStringToObject(response, "sessionIssueReason", login_session_reason_to_string(reason));
    }
    error = WEB_API_ERROR_OK;
    esp_event_post(WEB_SERVER_EVENT, WEB_SERVER_EVENT_AUTH_OK, NULL, 0, portMAX_DELAY);

send_response:
    esp_err_t send_error = webserver_data_code_response_to_special_client_not_log(
        response,
        WEB_API_METHOD_SYS_EXCHANGE_PROVISIONING_HANDOFF_RSP,
        now_resp_arg->seq,
        error,
        now_resp_arg->fd);
    login_session_secure_zero(&credential, sizeof(credential));
    return send_error;
}
