#include "ha_auth_api.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ha_auth.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mdns.h"
#include "webserver_api.h"
#include "webserver_client_node.h"
#include "webserver_method_api.h"

#define HASS_API_ERROR_INVALID_PARAM 920
#define HASS_API_ERROR_INVALID_URL 921
#define HASS_API_ERROR_OAUTH_BUSY 922
#define HASS_API_ERROR_STATE_MISMATCH 923
#define HASS_API_ERROR_STATE_EXPIRED 924
#define HASS_API_ERROR_CODE_EXCHANGE 925
#define HASS_API_ERROR_HA_CONNECTION 926
#define HASS_API_ERROR_LONG_LIVED_TOKEN 927
#define HASS_API_ERROR_STORAGE 928
#define HASS_API_ERROR_NOT_CONFIGURED 929
#define HASS_API_ERROR_INTERNAL 930
#define HASS_API_ERROR_AUTH_INVALID 931
#define HASS_MDNS_HOSTNAME_MAX_LEN 64

typedef struct {
    int socket_fd;
    char client_nonce[WEB_AUTH_NONCE_HEX_LEN + 1];
} ha_auth_web_result_context_t;

typedef struct {
    int socket_fd;
    double seq;
    char client_nonce[WEB_AUTH_NONCE_HEX_LEN + 1];
} ha_update_url_web_context_t;

static int ha_auth_reason_to_api_error(ha_auth_reason_t reason)
{
    switch (reason) {
    case HA_AUTH_REASON_NONE: return WEB_API_ERROR_OK;
    case HA_AUTH_REASON_INVALID_PARAM: return HASS_API_ERROR_INVALID_PARAM;
    case HA_AUTH_REASON_INVALID_URL: return HASS_API_ERROR_INVALID_URL;
    case HA_AUTH_REASON_OAUTH_BUSY: return HASS_API_ERROR_OAUTH_BUSY;
    case HA_AUTH_REASON_STATE_MISMATCH: return HASS_API_ERROR_STATE_MISMATCH;
    case HA_AUTH_REASON_STATE_EXPIRED: return HASS_API_ERROR_STATE_EXPIRED;
    case HA_AUTH_REASON_CODE_EXCHANGE_FAILED: return HASS_API_ERROR_CODE_EXCHANGE;
    case HA_AUTH_REASON_HA_WS_CONNECT_FAILED: return HASS_API_ERROR_HA_CONNECTION;
    case HA_AUTH_REASON_LONG_LIVED_TOKEN_FAILED: return HASS_API_ERROR_LONG_LIVED_TOKEN;
    case HA_AUTH_REASON_STORAGE_FAILED: return HASS_API_ERROR_STORAGE;
    case HA_AUTH_REASON_NOT_CONFIGURED: return HASS_API_ERROR_NOT_CONFIGURED;
    case HA_AUTH_REASON_AUTH_INVALID: return HASS_API_ERROR_AUTH_INVALID;
    case HA_AUTH_REASON_CONNECTION_ERROR: return HASS_API_ERROR_HA_CONNECTION;
    default: return HASS_API_ERROR_INTERNAL;
    }
}

static cJSON *ha_auth_create_public_status(const ha_auth_connection_info_t *info)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) return NULL;
    cJSON_AddBoolToObject(data, "configured", info->configured);
    cJSON_AddBoolToObject(data, "enabled", info->enabled);
    cJSON_AddBoolToObject(data, "deviceConnected", info->device_connected);
    cJSON_AddStringToObject(data, "status", ha_auth_status_to_string(info->status));
    cJSON_AddStringToObject(data, "reason", ha_auth_reason_to_string(info->reason));
    return data;
}

static cJSON *ha_auth_create_connection_info(const ha_auth_connection_info_t *info, bool include_token)
{
    cJSON *data = ha_auth_create_public_status(info);
    if (!data) return NULL;
    if (info->configured) {
        cJSON_AddStringToObject(data, "hassUrl", info->hass_url);
        cJSON_AddNumberToObject(data, "tokenGeneratedAt", (double)info->token_generated_at * 1000.0);
        if (include_token) {
            cJSON_AddStringToObject(data, "longLivedToken", info->long_lived_token);
        }
    }
    return data;
}

static bool ha_auth_extract_url_host(const char *url, char *host, size_t host_size)
{
    const char *authority = NULL;
    if (strncmp(url, "http://", 7) == 0) authority = url + 7;
    else if (strncmp(url, "https://", 8) == 0) authority = url + 8;
    else return false;

    const char *end = strpbrk(authority, "/?#");
    if (!end) end = authority + strlen(authority);
    if (authority == end || memchr(authority, '@', (size_t)(end - authority))) return false;

    const char *host_start = authority;
    const char *host_end = end;
    if (*host_start == '[') {
        host_start++;
        host_end = memchr(host_start, ']', (size_t)(end - host_start));
        if (!host_end) return false;
    }
    else {
        const char *colon = memchr(host_start, ':', (size_t)(end - host_start));
        if (colon) host_end = colon;
    }

    size_t len = (size_t)(host_end - host_start);
    if (len == 0 || len >= host_size) return false;
    memcpy(host, host_start, len);
    host[len] = '\0';
    for (size_t i = 0; i < len; i++) host[i] = (char)tolower((unsigned char)host[i]);
    return true;
}

static bool ha_auth_redirect_uri_is_local(const char *redirect_uri, int socket_fd)
{
    const char *authority = strstr(redirect_uri, "://");
    authority = authority ? authority + 3 : NULL;
    const char *path = authority ? strchr(authority, '/') : NULL;
    if (path && strcmp(path, "/") != 0) return false;

    char host[128] = {0};
    if (!ha_auth_extract_url_host(redirect_uri, host, sizeof(host))) return false;

    struct in_addr redirect_ipv4;
    if (inet_pton(AF_INET, host, &redirect_ipv4) == 1) {
        struct sockaddr_storage local_addr = {0};
        socklen_t local_len = sizeof(local_addr);
        if (getsockname(socket_fd, (struct sockaddr *)&local_addr, &local_len) != 0) return false;
        if (local_addr.ss_family == AF_INET) {
            return ((struct sockaddr_in *)&local_addr)->sin_addr.s_addr == redirect_ipv4.s_addr;
        }
        if (local_addr.ss_family == AF_INET6) {
            const uint8_t *bytes = ((struct sockaddr_in6 *)&local_addr)->sin6_addr.s6_addr;
            static const uint8_t mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
            return memcmp(bytes, mapped_prefix, sizeof(mapped_prefix)) == 0 &&
                   memcmp(bytes + sizeof(mapped_prefix), &redirect_ipv4.s_addr, sizeof(redirect_ipv4.s_addr)) == 0;
        }
        return false;
    }

    char mdns_hostname[HASS_MDNS_HOSTNAME_MAX_LEN] = {0};
    if (mdns_hostname_get(mdns_hostname) != ESP_OK || mdns_hostname[0] == '\0') return false;
    char mdns_fqdn[HASS_MDNS_HOSTNAME_MAX_LEN + 7] = {0};
    snprintf(mdns_fqdn, sizeof(mdns_fqdn), "%s.local", mdns_hostname);
    return strcasecmp(host, mdns_hostname) == 0 || strcasecmp(host, mdns_fqdn) == 0;
}

static bool ha_auth_web_client_matches(int socket_fd, const char *client_nonce)
{
    if (!client_nonce || !webserver_client_get_auth(socket_fd)) return false;
    char current_nonce[WEB_AUTH_NONCE_HEX_LEN + 1] = {0};
    return webserver_get_client_nonce(socket_fd, current_nonce) == ESP_OK &&
           strcmp(current_nonce, client_nonce) == 0;
}

static bool ha_auth_web_client_is_same(const ha_auth_web_result_context_t *context)
{
    return context && ha_auth_web_client_matches(context->socket_fd,
                                                  context->client_nonce);
}

static void ha_auth_web_authorization_result(esp_err_t err,
                                             ha_auth_reason_t reason,
                                             const ha_auth_connection_info_t *info,
                                             void *user_ctx)
{
    ha_auth_web_result_context_t *context = user_ctx;
    if (ha_auth_web_client_is_same(context)) {
        cJSON *data = cJSON_CreateObject();
        if (data) {
            if (err == ESP_OK) {
                cJSON_AddStringToObject(data, "status", "authorized");
                cJSON_AddStringToObject(data, "hassUrl", info->hass_url);
                cJSON_AddStringToObject(data, "longLivedToken", info->long_lived_token);
                cJSON_AddNumberToObject(data, "tokenGeneratedAt", (double)info->token_generated_at * 1000.0);
                cJSON_AddBoolToObject(data, "enabled", info->enabled);
                cJSON_AddBoolToObject(data, "deviceConnected", info->device_connected);
            }
            else {
                cJSON_AddStringToObject(data, "status", "error");
                cJSON_AddStringToObject(data, "reason", ha_auth_reason_to_string(reason));
            }
            webserver_data_response_to_special_client_not_log(data,
                                                               WEB_API_METHOD_HASS_AUTH_RESULT_NOTIFY,
                                                               0,
                                                               context->socket_fd);
        }
    }
    free(context);
}

static void ha_auth_web_update_url_result(esp_err_t err,
                                          ha_auth_reason_t reason,
                                          bool changed,
                                          const ha_auth_connection_info_t *info,
                                          void *user_ctx)
{
    ha_update_url_web_context_t *context = user_ctx;
    if (context && ha_auth_web_client_matches(context->socket_fd,
                                               context->client_nonce)) {
        cJSON *data = err == ESP_OK ? ha_auth_create_public_status(info)
                                    : cJSON_CreateObject();
        if (data) {
            if (err == ESP_OK) {
                cJSON_AddBoolToObject(data, "changed", changed);
                cJSON_AddStringToObject(data, "hassUrl", info->hass_url);
            }
            else {
                cJSON_AddStringToObject(data,
                                       "reason",
                                       ha_auth_reason_to_string(reason));
            }
            webserver_data_code_response_to_special_client_not_log(
                data,
                WEB_API_METHOD_HASS_UPDATE_URL_RSP,
                context->seq,
                err == ESP_OK ? WEB_API_ERROR_OK : ha_auth_reason_to_api_error(reason),
                context->socket_fd);
        }
    }
    free(context);
}

static void ha_auth_web_status_changed(const ha_auth_connection_info_t *info, void *user_ctx)
{
    (void)user_ctx;
    cJSON *data = ha_auth_create_public_status(info);
    if (data) {
        webserver_data_response_to_authenticated_clients(data, WEB_API_METHOD_HASS_CONNECTION_STATUS_NOTIFY, 0);
    }
}

void ha_auth_api_init(void)
{
    ha_auth_set_status_callback(ha_auth_web_status_changed, NULL);
}

esp_err_t method_api_handle_hass_start_oauth(cJSON *data, async_resp_arg *now_resp_arg)
{
    const cJSON *url_item = cJSON_GetObjectItemCaseSensitive(data, "hassUrl");
    const cJSON *redirect_item = cJSON_GetObjectItemCaseSensitive(data, "redirectUri");
    int error = HASS_API_ERROR_INVALID_PARAM;
    cJSON *response = NULL;

    if (cJSON_IsString(url_item) && url_item->valuestring && cJSON_IsString(redirect_item) &&
        redirect_item->valuestring && ha_auth_redirect_uri_is_local(redirect_item->valuestring, now_resp_arg->fd)) {
        char *authorize_url = calloc(1, HA_AUTH_AUTHORIZE_URL_MAX_LEN);
        uint32_t expires_in = 0;
        ha_auth_reason_t reason = HA_AUTH_REASON_NONE;
        if (authorize_url) {
            esp_err_t err = ha_auth_start_oauth(url_item->valuestring,
                                                redirect_item->valuestring,
                                                authorize_url,
                                                HA_AUTH_AUTHORIZE_URL_MAX_LEN,
                                                &expires_in,
                                                &reason);
            error = err == ESP_OK ? WEB_API_ERROR_OK : ha_auth_reason_to_api_error(reason);
            if (err == ESP_OK) {
                response = cJSON_CreateObject();
                if (response) {
                    cJSON_AddStringToObject(response, "authorizeUrl", authorize_url);
                    cJSON_AddNumberToObject(response, "expiresIn", expires_in);
                }
            }
            free(authorize_url);
        }
        else {
            error = HASS_API_ERROR_INTERNAL;
        }
    }
    else if (cJSON_IsString(redirect_item)) {
        error = HASS_API_ERROR_INVALID_URL;
    }

    return webserver_data_code_response_to_special_client_not_log(response,
                                                                   WEB_API_METHOD_HASS_START_OAUTH_RSP,
                                                                   now_resp_arg->seq,
                                                                   error,
                                                                   now_resp_arg->fd);
}

esp_err_t method_api_handle_hass_complete_oauth(cJSON *data, async_resp_arg *now_resp_arg)
{
    const cJSON *code_item = cJSON_GetObjectItemCaseSensitive(data, "code");
    const cJSON *state_item = cJSON_GetObjectItemCaseSensitive(data, "state");
    int error = HASS_API_ERROR_INVALID_PARAM;

    if (cJSON_IsString(code_item) && code_item->valuestring && cJSON_IsString(state_item) && state_item->valuestring) {
        ha_auth_web_result_context_t *context = calloc(1, sizeof(*context));
        if (context) {
            context->socket_fd = now_resp_arg->fd;
            if (webserver_get_client_nonce(context->socket_fd, context->client_nonce) == ESP_OK) {
                ha_auth_reason_t reason = HA_AUTH_REASON_NONE;
                esp_err_t err = ha_auth_complete_oauth(code_item->valuestring,
                                                       state_item->valuestring,
                                                       ha_auth_web_authorization_result,
                                                       context,
                                                       &reason);
                error = err == ESP_OK ? WEB_API_ERROR_OK : ha_auth_reason_to_api_error(reason);
                if (err == ESP_OK) context = NULL;
            }
            else {
                error = WEB_API_ERROR_AUTH;
            }
            free(context);
        }
        else {
            error = HASS_API_ERROR_INTERNAL;
        }
    }

    return webserver_data_code_response_to_special_client_not_log(NULL,
                                                                   WEB_API_METHOD_HASS_COMPLETE_OAUTH_RSP,
                                                                   now_resp_arg->seq,
                                                                   error,
                                                                   now_resp_arg->fd);
}

esp_err_t method_api_handle_hass_get_connection_info(cJSON *data, async_resp_arg *now_resp_arg)
{
    (void)data;
    ha_auth_connection_info_t info = {0};
    esp_err_t err = ha_auth_get_connection_info(&info);
    cJSON *response = err == ESP_OK ? ha_auth_create_connection_info(&info, true) : NULL;
    esp_err_t send_err = webserver_data_code_response_to_special_client_not_log(
        response,
        WEB_API_METHOD_HASS_GET_CONNECTION_INFO_RSP,
        now_resp_arg->seq,
        err == ESP_OK ? WEB_API_ERROR_OK : HASS_API_ERROR_INTERNAL,
        now_resp_arg->fd);
    memset(info.long_lived_token, 0, sizeof(info.long_lived_token));
    return send_err;
}

esp_err_t method_api_handle_hass_update_url(cJSON *data, async_resp_arg *now_resp_arg)
{
    const cJSON *url_item = cJSON_GetObjectItemCaseSensitive(data, "hassUrl");
    int error = HASS_API_ERROR_INVALID_PARAM;
    ha_auth_reason_t reason = HA_AUTH_REASON_INVALID_PARAM;

    if (cJSON_IsString(url_item) && url_item->valuestring) {
        ha_update_url_web_context_t *context = calloc(1, sizeof(*context));
        if (!context) {
            error = HASS_API_ERROR_INTERNAL;
        }
        else {
            context->socket_fd = now_resp_arg->fd;
            context->seq = now_resp_arg->seq;
            if (webserver_get_client_nonce(context->socket_fd,
                                           context->client_nonce) != ESP_OK) {
                error = WEB_API_ERROR_AUTH;
            }
            else {
                esp_err_t err = ha_auth_update_url(url_item->valuestring,
                                                   ha_auth_web_update_url_result,
                                                   context,
                                                   &reason);
                if (err == ESP_OK) {
                    return ESP_OK;
                }
                error = ha_auth_reason_to_api_error(reason);
            }
            free(context);
        }
    }

    cJSON *response = cJSON_CreateObject();
    if (response) {
        const char *reason_text = error == HASS_API_ERROR_INTERNAL ? "internalError" :
                                  error == WEB_API_ERROR_AUTH ? "unauthorized" :
                                  ha_auth_reason_to_string(reason);
        cJSON_AddStringToObject(response, "reason", reason_text);
    }
    return webserver_data_code_response_to_special_client_not_log(response,
                                                                   WEB_API_METHOD_HASS_UPDATE_URL_RSP,
                                                                   now_resp_arg->seq,
                                                                   error,
                                                                   now_resp_arg->fd);
}

esp_err_t method_api_handle_hass_disable(cJSON *data, async_resp_arg *now_resp_arg)
{
    (void)data;
    esp_err_t err = ha_auth_disable();
    ha_auth_connection_info_t info = {0};
    ha_auth_get_connection_info(&info);
    cJSON *response = ha_auth_create_public_status(&info);
    int error = err == ESP_OK ? WEB_API_ERROR_OK :
                (err == ESP_ERR_INVALID_STATE ? HASS_API_ERROR_OAUTH_BUSY : HASS_API_ERROR_STORAGE);
    return webserver_data_code_response_to_special_client_not_log(response,
                                                                   WEB_API_METHOD_HASS_DISABLE_RSP,
                                                                   now_resp_arg->seq,
                                                                   error,
                                                                   now_resp_arg->fd);
}

esp_err_t method_api_handle_hass_activate(cJSON *data, async_resp_arg *now_resp_arg)
{
    (void)data;
    ha_auth_reason_t reason = HA_AUTH_REASON_NONE;
    esp_err_t err = ha_auth_activate(&reason);
    ha_auth_connection_info_t info = {0};
    ha_auth_get_connection_info(&info);
    cJSON *response = ha_auth_create_public_status(&info);
    return webserver_data_code_response_to_special_client_not_log(response,
                                                                   WEB_API_METHOD_HASS_ACTIVATE_RSP,
                                                                   now_resp_arg->seq,
                                                                   err == ESP_OK ? WEB_API_ERROR_OK : ha_auth_reason_to_api_error(reason),
                                                                   now_resp_arg->fd);
}

esp_err_t method_api_handle_hass_remove_authorization(cJSON *data, async_resp_arg *now_resp_arg)
{
    (void)data;
    esp_err_t err = ha_auth_remove_authorization();
    int error = err == ESP_OK ? WEB_API_ERROR_OK :
                (err == ESP_ERR_INVALID_STATE ? HASS_API_ERROR_OAUTH_BUSY : HASS_API_ERROR_STORAGE);
    return webserver_data_code_response_to_special_client_not_log(NULL,
                                                                   WEB_API_METHOD_HASS_REMOVE_AUTH_RSP,
                                                                   now_resp_arg->seq,
                                                                   error,
                                                                   now_resp_arg->fd);
}
