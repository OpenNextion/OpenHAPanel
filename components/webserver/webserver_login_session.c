#include "login_session_api.h"

#include <string.h>

#include "login_session.h"
#include "webserver.h"
#include "webserver_api.h"
#include "webserver_auth.h"
#include "webserver_client_node.h"
#include "webserver_method_api.h"

static int login_session_reason_to_api_error(login_session_reason_t reason)
{
    switch (reason) {
    case LOGIN_SESSION_REASON_EXPIRED: return WEB_API_ERROR_SESSION_EXPIRED;
    case LOGIN_SESSION_REASON_TIME_UNAVAILABLE: return WEB_API_ERROR_SESSION_TIME_UNAVAILABLE;
    case LOGIN_SESSION_REASON_STORAGE_FAILED: return WEB_API_ERROR_SESSION_STORAGE;
    default: return WEB_API_ERROR_SESSION_INVALID;
    }
}

static void send_fresh_auth_challenge(async_resp_arg *resp_arg, const webserver_client_node_t *client)
{
    if (resp_arg && client && !client->is_ap) {
        webserver_login_authentication(resp_arg, false, 0);
    }
}

esp_err_t method_api_handle_resume_session(cJSON *data, async_resp_arg *now_resp_arg)
{
    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(data, "sessionId");
    const cJSON *cnonce_item = cJSON_GetObjectItemCaseSensitive(data, "cnonce");
    const cJSON *proof_item = cJSON_GetObjectItemCaseSensitive(data, "proof");
    webserver_client_node_t client = {0};
    int error = WEB_API_ERROR_SESSION_INVALID;
    cJSON *response = NULL;

    if (!webserver_client_find_copy(now_resp_arg->fd, &client) || client.is_ap || client.auth) {
        goto send_response;
    }
    if (!webserver_client_auth_attempt_allowed(now_resp_arg->fd)) {
        error = WEB_API_ERROR_SESSION_TOO_MANY_ATTEMPTS;
        goto send_response;
    }
    if (!cJSON_IsString(id_item) || !id_item->valuestring ||
        !cJSON_IsString(cnonce_item) || !cnonce_item->valuestring ||
        !cJSON_IsString(proof_item) || !proof_item->valuestring) {
        webserver_client_record_auth_failure(now_resp_arg->fd);
        goto send_response;
    }

    uint32_t expires_at = 0;
    login_session_reason_t reason = LOGIN_SESSION_REASON_INVALID;
    esp_err_t err = login_session_verify(id_item->valuestring,
                                         client.nonce,
                                         cnonce_item->valuestring,
                                         proof_item->valuestring,
                                         &expires_at,
                                         &reason);
    if (err == ESP_OK) {
        char rotated_nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};
        if (webserver_auth_rotate_nonce(now_resp_arg->fd,
                                        rotated_nonce,
                                        sizeof(rotated_nonce)) != ESP_OK) {
            error = WEB_API_ERROR_FAIL;
            goto send_response;
        }
        response = cJSON_CreateObject();
        if (!response) {
            error = WEB_API_ERROR_FAIL;
        }
        else {
            cJSON_AddBoolToObject(response, "authenticated", true);
            cJSON_AddStringToObject(response, "nonce", rotated_nonce);
            cJSON_AddNumberToObject(response, "expiresAt", (double)expires_at * 1000.0);
            webserver_client_set_auth_context(now_resp_arg->fd,
                                              true,
                                              WEB_AUTH_SOURCE_SESSION,
                                              id_item->valuestring);
            error = WEB_API_ERROR_OK;
            esp_event_post(WEB_SERVER_EVENT, WEB_SERVER_EVENT_AUTH_OK, NULL, 0, portMAX_DELAY);
        }
    }
    else {
        webserver_client_record_auth_failure(now_resp_arg->fd);
        error = login_session_reason_to_api_error(reason);
    }

send_response:
    if (!response) {
        response = cJSON_CreateObject();
        if (response) {
            cJSON_AddBoolToObject(response, "authenticated", false);
            cJSON_AddStringToObject(response,
                                    "reason",
                                    error == WEB_API_ERROR_SESSION_EXPIRED ? "sessionExpired" :
                                    error == WEB_API_ERROR_SESSION_TIME_UNAVAILABLE ? "timeUnavailable" :
                                    error == WEB_API_ERROR_SESSION_TOO_MANY_ATTEMPTS ? "tooManyAttempts" :
                                    "sessionInvalid");
        }
    }
    esp_err_t send_err = webserver_data_code_response_to_special_client_not_log(
        response,
        WEB_API_METHOD_SYS_RESUME_SESSION_RSP,
        now_resp_arg->seq,
        error,
        now_resp_arg->fd);
    if (error != WEB_API_ERROR_OK &&
        error != WEB_API_ERROR_SESSION_TOO_MANY_ATTEMPTS &&
        webserver_client_find_copy(now_resp_arg->fd, &client)) {
        send_fresh_auth_challenge(now_resp_arg, &client);
    }
    return send_err;
}

esp_err_t method_api_handle_logout(cJSON *data, async_resp_arg *now_resp_arg)
{
    (void)data;
    webserver_client_node_t client = {0};
    if (!webserver_client_find_copy(now_resp_arg->fd, &client)) {
        return webserver_data_code_response_to_special_client_not_log(
            NULL, WEB_API_METHOD_SYS_LOGOUT_RSP, now_resp_arg->seq,
            WEB_API_ERROR_FAIL, now_resp_arg->fd);
    }

    int error = WEB_API_ERROR_OK;
    if (client.session_id[0] != '\0') {
        esp_err_t err = login_session_revoke(client.session_id);
        if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) error = WEB_API_ERROR_SESSION_STORAGE;
    }
    esp_err_t send_err = webserver_data_code_response_to_special_client_not_log(
        NULL, WEB_API_METHOD_SYS_LOGOUT_RSP, now_resp_arg->seq, error, now_resp_arg->fd);
    if (error == WEB_API_ERROR_OK && !client.is_ap) {
        webserver_client_set_auth_context(now_resp_arg->fd, false, WEB_AUTH_SOURCE_NONE, NULL);
        send_fresh_auth_challenge(now_resp_arg, &client);
    }
    return send_err;
}

esp_err_t method_api_handle_revoke_all_sessions(cJSON *data, async_resp_arg *now_resp_arg)
{
    (void)data;
    webserver_client_node_t client = {0};
    bool has_client = webserver_client_find_copy(now_resp_arg->fd, &client);
    esp_err_t err = login_session_revoke_all();
    if (err == ESP_OK) {
        if (has_client) {
            webserver_client_set_auth_context(now_resp_arg->fd,
                                              true,
                                              client.is_ap ? WEB_AUTH_SOURCE_AP : WEB_AUTH_SOURCE_PASSWORD,
                                              NULL);
        }
        webserver_close_other_wifi_and_eth_clients(now_resp_arg->fd);
    }
    return webserver_data_code_response_to_special_client_not_log(
        NULL,
        WEB_API_METHOD_SYS_REVOKE_ALL_SESSIONS_RSP,
        now_resp_arg->seq,
        err == ESP_OK ? WEB_API_ERROR_OK : WEB_API_ERROR_SESSION_STORAGE,
        now_resp_arg->fd);
}
