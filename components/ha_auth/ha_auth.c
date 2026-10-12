#include "ha_auth.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ha_ws.h"
#include "nvs_storage.h"

#define HA_AUTH_NVS_NAMESPACE "ha_auth"
#define HA_AUTH_NVS_KEY_URL "ha_url"
#define HA_AUTH_NVS_KEY_TOKEN "ll_token"
#define HA_AUTH_NVS_KEY_TOKEN_TIME "token_time"
#define HA_AUTH_NVS_KEY_ENABLED "enabled"

#define HA_AUTH_STATE_BYTES 32
#define HA_AUTH_STATE_HEX_LEN (HA_AUTH_STATE_BYTES * 2)
#define HA_AUTH_CODE_MAX_LEN 1024
#define HA_AUTH_HTTP_RESPONSE_MAX_LEN 4096
#define HA_AUTH_OAUTH_TIMEOUT_SECONDS 300
#define HA_AUTH_HTTP_TIMEOUT_MS 15000
#define HA_AUTH_WS_TIMEOUT_MS 20000
#define HA_AUTH_UPDATE_URL_SLOT_TIMEOUT_MS 3000
#define HA_AUTH_UPDATE_URL_VERIFY_TIMEOUT_MS 12000
#define HA_AUTH_WORKER_STACK_SIZE (16 * 1024)
#define HA_AUTH_UPDATE_URL_WORKER_STACK_SIZE (6 * 1024)
#define HA_AUTH_LONG_LIVED_LIFESPAN_DAYS 3650
#define HA_AUTH_LONG_LIVED_CLIENT_NAME_PREFIX "OpenHAPanel"
#define HA_AUTH_LONG_LIVED_CLIENT_NAME_MAX_LEN \
    sizeof(HA_AUTH_LONG_LIVED_CLIENT_NAME_PREFIX " FFFF")
#define HA_AUTH_MAX_STALE_TOKEN_DELETIONS 4
#define HA_AUTH_VALID_EPOCH_MIN 1577836800UL

#define HA_AUTH_WORKER_EVENT_SUCCESS BIT0
#define HA_AUTH_WORKER_EVENT_FAILED BIT1

ESP_EVENT_DEFINE_BASE(HA_AUTH_EVENT);

typedef struct {
    bool pending;
    char state[HA_AUTH_STATE_HEX_LEN + 1];
    char hass_url[HA_AUTH_URL_MAX_LEN];
    char client_id[HA_AUTH_URL_MAX_LEN];
    char redirect_uri[HA_AUTH_URL_MAX_LEN];
    int64_t expires_at_us;
} ha_oauth_session_t;

typedef struct {
    char hass_url[HA_AUTH_URL_MAX_LEN];
    char code[HA_AUTH_CODE_MAX_LEN];
    char client_id[HA_AUTH_URL_MAX_LEN];
    ha_auth_authorization_cb_t callback;
    void *user_ctx;
} ha_auth_worker_args_t;

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool overflow;
} ha_http_response_t;

typedef struct {
    EventGroupHandle_t events;
    char long_lived_token[HA_AUTH_TOKEN_MAX_LEN];
    char client_name[HA_AUTH_LONG_LIVED_CLIENT_NAME_MAX_LEN];
    uint8_t stale_tokens_deleted;
    bool command_sent;
} ha_long_lived_token_context_t;

typedef struct {
    EventGroupHandle_t events;
    ha_auth_reason_t reason;
    bool terminal;
} ha_verify_context_t;

typedef struct {
    char hass_url[HA_AUTH_URL_MAX_LEN];
    char token[HA_AUTH_TOKEN_MAX_LEN];
    bool enabled;
    ha_auth_update_url_cb_t callback;
    void *user_ctx;
} ha_update_url_worker_args_t;

typedef struct {
    SemaphoreHandle_t lock;
    SemaphoreHandle_t operation_lock;
    ha_oauth_session_t oauth;
    ha_auth_connection_info_t info;
    bool worker_running;
    bool factory_resetting;
    ha_auth_status_cb_t status_callback;
    void *status_user_ctx;
} ha_auth_context_t;

static const char *TAG = "ha_auth";
static ha_auth_context_t s_ha_auth;

static esp_err_t ha_auth_post_event(ha_auth_event_id_t event_id)
{
    esp_err_t err = esp_event_post(HA_AUTH_EVENT,
                                   event_id,
                                   NULL,
                                   0,
                                   pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post HA auth event %d: %s", event_id, esp_err_to_name(err));
    }
    return err;
}

static void ha_auth_secure_zero(void *data, size_t length)
{
    volatile uint8_t *p = data;
    while (p && length--) {
        *p++ = 0;
    }
}

static esp_err_t ha_auth_build_long_lived_client_name(char *name,
                                                       size_t name_size)
{
    if (!name || name_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read Wi-Fi STA MAC for HA token name: %s",
                 esp_err_to_name(err));
        return err;
    }

    int written = snprintf(name,
                           name_size,
                           HA_AUTH_LONG_LIVED_CLIENT_NAME_PREFIX " %02X%02X",
                           (unsigned int)mac[4],
                           (unsigned int)mac[5]);
    if (written < 0 || (size_t)written >= name_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static void ha_auth_copy_info_locked(ha_auth_connection_info_t *info)
{
    memcpy(info, &s_ha_auth.info, sizeof(*info));
}

static void ha_auth_publish_status_with_reason(ha_auth_status_t status,
                                               ha_auth_reason_t reason)
{
    ha_auth_connection_info_t snapshot = {0};
    ha_auth_status_cb_t callback = NULL;
    void *user_ctx = NULL;

    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    s_ha_auth.info.status = status;
    s_ha_auth.info.reason = reason;
    ha_auth_copy_info_locked(&snapshot);
    callback = s_ha_auth.status_callback;
    user_ctx = s_ha_auth.status_user_ctx;
    xSemaphoreGive(s_ha_auth.lock);

    if (callback) {
        callback(&snapshot, user_ctx);
    }
    ha_auth_post_event(HA_AUTH_EVENT_STATUS_CHANGED);
    ha_auth_secure_zero(snapshot.long_lived_token, sizeof(snapshot.long_lived_token));
}

static void ha_auth_publish_status(ha_auth_status_t status)
{
    ha_auth_publish_status_with_reason(status, HA_AUTH_REASON_NONE);
}

const char *ha_auth_status_to_string(ha_auth_status_t status)
{
    switch (status) {
    case HA_AUTH_STATUS_NOT_CONFIGURED: return "notConfigured";
    case HA_AUTH_STATUS_AUTHORIZING: return "authorizing";
    case HA_AUTH_STATUS_EXCHANGING_CODE: return "exchangingCode";
    case HA_AUTH_STATUS_CREATING_LONG_LIVED_TOKEN: return "creatingLongLivedToken";
    case HA_AUTH_STATUS_DISABLED: return "disabled";
    case HA_AUTH_STATUS_CONNECTING: return "connecting";
    case HA_AUTH_STATUS_CONNECTED: return "connected";
    case HA_AUTH_STATUS_DISCONNECTED: return "disconnected";
    case HA_AUTH_STATUS_ERROR: return "error";
    default: return "error";
    }
}

const char *ha_auth_reason_to_string(ha_auth_reason_t reason)
{
    switch (reason) {
    case HA_AUTH_REASON_NONE: return "none";
    case HA_AUTH_REASON_INVALID_PARAM: return "invalidParam";
    case HA_AUTH_REASON_INVALID_URL: return "invalidUrl";
    case HA_AUTH_REASON_OAUTH_BUSY: return "oauthBusy";
    case HA_AUTH_REASON_STATE_MISMATCH: return "stateMismatch";
    case HA_AUTH_REASON_STATE_EXPIRED: return "stateExpired";
    case HA_AUTH_REASON_CODE_EXCHANGE_FAILED: return "codeExchangeFailed";
    case HA_AUTH_REASON_HA_WS_CONNECT_FAILED: return "haConnectionFailed";
    case HA_AUTH_REASON_LONG_LIVED_TOKEN_FAILED: return "longLivedTokenCreationFailed";
    case HA_AUTH_REASON_STORAGE_FAILED: return "tokenStorageFailed";
    case HA_AUTH_REASON_NOT_CONFIGURED: return "notConfigured";
    case HA_AUTH_REASON_AUTH_INVALID: return "authInvalid";
    case HA_AUTH_REASON_CONNECTION_ERROR: return "connectionError";
    default: return "unknown";
    }
}

static bool ha_auth_validate_port(const char *port, size_t length)
{
    if (!port || length == 0 || length > 5) return false;
    uint32_t value = 0;
    for (size_t i = 0; i < length; i++) {
        if (!isdigit((unsigned char)port[i])) return false;
        value = value * 10U + (uint32_t)(port[i] - '0');
    }
    return value >= 1 && value <= 65535;
}

static bool ha_auth_validate_url(const char *url)
{
    if (!url) {
        return false;
    }
    size_t len = strlen(url);
    if (len == 0 || len >= HA_AUTH_URL_MAX_LEN || strchr(url, '#') || strchr(url, '?')) {
        return false;
    }

    size_t scheme_len = 0;
    if (strncmp(url, "http://", 7) == 0) {
        scheme_len = 7;
    }
    else if (strncmp(url, "https://", 8) == 0) {
        scheme_len = 8;
    }
    else {
        return false;
    }

    if (strchr(url, '\\')) return false;

    const char *authority = url + scheme_len;
    const char *authority_end = strchr(authority, '/');
    size_t authority_len = authority_end ? (size_t)(authority_end - authority) : strlen(authority);
    if (authority_len == 0 || memchr(authority, '@', authority_len)) {
        return false;
    }

    const char *host_end = authority + authority_len;
    const char *port = NULL;
    size_t port_len = 0;
    if (authority[0] == '[') {
        const char *close = memchr(authority + 1, ']', authority_len - 1);
        if (!close || close == authority + 1) return false;
        const char *after = close + 1;
        if (after < authority + authority_len) {
            if (*after != ':') return false;
            port = after + 1;
            port_len = (size_t)((authority + authority_len) - port);
        }
        host_end = close + 1;
    }
    else {
        if (memchr(authority, '[', authority_len) || memchr(authority, ']', authority_len)) return false;
        const char *colon = memchr(authority, ':', authority_len);
        if (colon) {
            if (memchr(colon + 1, ':', (size_t)((authority + authority_len) - (colon + 1)))) return false;
            host_end = colon;
            port = colon + 1;
            port_len = (size_t)((authority + authority_len) - port);
        }
    }
    if (host_end == authority || (port && !ha_auth_validate_port(port, port_len))) return false;

    static const char websocket_suffix[] = "/api/websocket";
    size_t trimmed_len = len;
    while (trimmed_len > scheme_len && url[trimmed_len - 1] == '/') trimmed_len--;
    if (trimmed_len >= sizeof(websocket_suffix) - 1 &&
        memcmp(url + trimmed_len - (sizeof(websocket_suffix) - 1),
               websocket_suffix,
               sizeof(websocket_suffix) - 1) == 0) {
        return false;
    }

    for (const unsigned char *p = (const unsigned char *)url; *p; p++) {
        if (iscntrl(*p) || isspace(*p)) {
            return false;
        }
    }
    return true;
}

static void ha_auth_normalize_url(const char *url, char *out, size_t out_size)
{
    strlcpy(out, url, out_size);
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '/') {
        out[--len] = '\0';
    }
}

static bool ha_auth_constant_time_equal(const char *a, const char *b)
{
    if (!a || !b) {
        return false;
    }
    size_t a_len = strlen(a);
    size_t b_len = strlen(b);
    size_t max_len = a_len > b_len ? a_len : b_len;
    unsigned char diff = (unsigned char)(a_len ^ b_len);
    for (size_t i = 0; i < max_len; i++) {
        unsigned char av = i < a_len ? (unsigned char)a[i] : 0;
        unsigned char bv = i < b_len ? (unsigned char)b[i] : 0;
        diff |= av ^ bv;
    }
    return diff == 0;
}

static void ha_auth_generate_state(char state[HA_AUTH_STATE_HEX_LEN + 1])
{
    static const char hex[] = "0123456789abcdef";
    uint8_t random[HA_AUTH_STATE_BYTES];
    esp_fill_random(random, sizeof(random));
    for (size_t i = 0; i < sizeof(random); i++) {
        state[i * 2] = hex[random[i] >> 4];
        state[i * 2 + 1] = hex[random[i] & 0x0f];
    }
    state[HA_AUTH_STATE_HEX_LEN] = '\0';
    ha_auth_secure_zero(random, sizeof(random));
}

static char *ha_auth_url_encode(const char *value)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t len = strlen(value);
    char *encoded = malloc(len * 3 + 1);
    if (!encoded) {
        return NULL;
    }

    char *out = encoded;
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            *out++ = (char)*p;
        }
        else {
            *out++ = '%';
            *out++ = hex[*p >> 4];
            *out++ = hex[*p & 0x0f];
        }
    }
    *out = '\0';
    return encoded;
}

static char *ha_auth_build_endpoint(const char *hass_url, const char *path)
{
    size_t url_len = strlen(hass_url);
    size_t path_len = strlen(path);
    bool needs_slash = url_len == 0 || hass_url[url_len - 1] != '/';
    char *url = malloc(url_len + path_len + (needs_slash ? 2 : 1));
    if (!url) {
        return NULL;
    }
    snprintf(url, url_len + path_len + (needs_slash ? 2 : 1), "%s%s%s", hass_url, needs_slash ? "/" : "", path);
    return url;
}

static esp_err_t ha_auth_http_event(esp_http_client_event_t *event)
{
    ha_http_response_t *response = event ? event->user_data : NULL;
    if (!response || event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
        return ESP_OK;
    }

    if (response->length + (size_t)event->data_len >= response->capacity) {
        response->overflow = true;
        return ESP_OK;
    }
    memcpy(response->data + response->length, event->data, event->data_len);
    response->length += event->data_len;
    response->data[response->length] = '\0';
    return ESP_OK;
}

static esp_err_t ha_auth_http_post_form(const char *url,
                                        const char *form,
                                        char *response_buffer,
                                        size_t response_size,
                                        int *status_code)
{
    ha_http_response_t response = {
        .data = response_buffer,
        .capacity = response_size,
    };
    response_buffer[0] = '\0';

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = ha_auth_http_event,
        .user_data = &response,
        .timeout_ms = HA_AUTH_HTTP_TIMEOUT_MS,
        .buffer_size = 1024,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, form, strlen(form));
    esp_err_t err = esp_http_client_perform(client);
    if (status_code) {
        *status_code = esp_http_client_get_status_code(client);
    }
    esp_http_client_cleanup(client);

    if (response.overflow) {
        return ESP_ERR_INVALID_SIZE;
    }
    return err;
}

static esp_err_t ha_auth_exchange_code(const char *hass_url,
                                       const char *code,
                                       const char *client_id,
                                       char *access_token,
                                       size_t access_token_size,
                                       char *refresh_token,
                                       size_t refresh_token_size)
{
    esp_err_t ret = ESP_FAIL;
    char *url = ha_auth_build_endpoint(hass_url, "auth/token");
    char *encoded_code = ha_auth_url_encode(code);
    char *encoded_client_id = ha_auth_url_encode(client_id);
    char *form = NULL;
    char *response = calloc(1, HA_AUTH_HTTP_RESPONSE_MAX_LEN);
    cJSON *root = NULL;

    if (!url || !encoded_code || !encoded_client_id || !response) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    size_t form_size = strlen(encoded_code) + strlen(encoded_client_id) + 64;
    form = malloc(form_size);
    if (!form) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    snprintf(form, form_size, "grant_type=authorization_code&code=%s&client_id=%s", encoded_code, encoded_client_id);

    int status = 0;
    ret = ha_auth_http_post_form(url, form, response, HA_AUTH_HTTP_RESPONSE_MAX_LEN, &status);
    if (ret != ESP_OK || status < 200 || status >= 300) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    root = cJSON_Parse(response);
    const cJSON *access = root ? cJSON_GetObjectItemCaseSensitive(root, "access_token") : NULL;
    const cJSON *refresh = root ? cJSON_GetObjectItemCaseSensitive(root, "refresh_token") : NULL;
    const cJSON *type = root ? cJSON_GetObjectItemCaseSensitive(root, "token_type") : NULL;
    if (!cJSON_IsString(access) || !access->valuestring || access->valuestring[0] == '\0' ||
        !cJSON_IsString(refresh) || !refresh->valuestring || refresh->valuestring[0] == '\0' ||
        !cJSON_IsString(type) || strcasecmp(type->valuestring, "Bearer") != 0 ||
        strlen(access->valuestring) >= access_token_size || strlen(refresh->valuestring) >= refresh_token_size) {
        ret = ESP_ERR_INVALID_RESPONSE;
        goto cleanup;
    }

    strlcpy(access_token, access->valuestring, access_token_size);
    strlcpy(refresh_token, refresh->valuestring, refresh_token_size);
    ret = ESP_OK;

cleanup:
    cJSON_Delete(root);
    if (form) ha_auth_secure_zero(form, strlen(form));
    if (response) ha_auth_secure_zero(response, HA_AUTH_HTTP_RESPONSE_MAX_LEN);
    if (encoded_code) ha_auth_secure_zero(encoded_code, strlen(encoded_code));
    free(form);
    free(response);
    free(encoded_code);
    free(encoded_client_id);
    free(url);
    return ret;
}

static void ha_auth_revoke_refresh_token(const char *hass_url, const char *refresh_token)
{
    char *url = ha_auth_build_endpoint(hass_url, "auth/revoke");
    char *encoded = ha_auth_url_encode(refresh_token);
    char response[256] = {0};
    if (!url || !encoded) {
        free(url);
        free(encoded);
        return;
    }

    size_t form_size = strlen(encoded) + 16;
    char *form = malloc(form_size);
    if (form) {
        snprintf(form, form_size, "token=%s", encoded);
        int status = 0;
        esp_err_t err = ha_auth_http_post_form(url, form, response, sizeof(response), &status);
        if (err != ESP_OK || status < 200 || status >= 300) {
            ESP_LOGW(TAG, "Failed to revoke temporary OAuth credentials");
        }
        ha_auth_secure_zero(form, strlen(form));
        free(form);
    }
    ha_auth_secure_zero(response, sizeof(response));
    ha_auth_secure_zero(encoded, strlen(encoded));
    free(encoded);
    free(url);
}

static void ha_auth_long_lived_result_cb(ha_ws_client_t *client,
                                         int id,
                                         esp_err_t err,
                                         const cJSON *result,
                                         const cJSON *error,
                                         void *user_ctx)
{
    (void)client;
    (void)id;
    (void)error;
    ha_long_lived_token_context_t *context = user_ctx;
    if (err == ESP_OK && cJSON_IsString(result) && result->valuestring && result->valuestring[0] != '\0' &&
        strlen(result->valuestring) < sizeof(context->long_lived_token)) {
        strlcpy(context->long_lived_token, result->valuestring, sizeof(context->long_lived_token));
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_SUCCESS);
    }
    else {
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
    }
}

static void ha_auth_refresh_tokens_result_cb(ha_ws_client_t *client,
                                             int id,
                                             esp_err_t err,
                                             const cJSON *result,
                                             const cJSON *error,
                                             void *user_ctx);

static esp_err_t ha_auth_send_refresh_tokens_query(
    ha_ws_client_t *client,
    ha_long_lived_token_context_t *context)
{
    cJSON *command = cJSON_CreateObject();
    if (!command) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(command, "type", "auth/refresh_tokens");
    esp_err_t err = ha_ws_send_command(client,
                                       command,
                                       ha_auth_refresh_tokens_result_cb,
                                       context,
                                       NULL);
    cJSON_Delete(command);
    return err;
}

static esp_err_t ha_auth_send_long_lived_token_create(
    ha_ws_client_t *client,
    ha_long_lived_token_context_t *context)
{
    cJSON *command = cJSON_CreateObject();
    if (!command) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(command,
                            "type",
                            "auth/long_lived_access_token");
    cJSON_AddStringToObject(command, "client_name", context->client_name);
    cJSON_AddNumberToObject(command,
                           "lifespan",
                           HA_AUTH_LONG_LIVED_LIFESPAN_DAYS);
    esp_err_t err = ha_ws_send_command(client,
                                       command,
                                       ha_auth_long_lived_result_cb,
                                       context,
                                       NULL);
    cJSON_Delete(command);
    return err;
}

static void ha_auth_delete_stale_token_result_cb(ha_ws_client_t *client,
                                                  int id,
                                                  esp_err_t err,
                                                  const cJSON *result,
                                                  const cJSON *error,
                                                  void *user_ctx)
{
    (void)id;
    (void)result;
    (void)error;
    ha_long_lived_token_context_t *context = user_ctx;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete stale HA long-lived token");
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
        return;
    }

    /* Query again so an unexpected duplicate is also removed before create. */
    err = ha_auth_send_refresh_tokens_query(client, context);
    if (err != ESP_OK) {
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
    }
}

static void ha_auth_refresh_tokens_result_cb(ha_ws_client_t *client,
                                             int id,
                                             esp_err_t err,
                                             const cJSON *result,
                                             const cJSON *error,
                                             void *user_ctx)
{
    (void)id;
    (void)error;
    ha_long_lived_token_context_t *context = user_ctx;
    if (err != ESP_OK || !cJSON_IsArray(result)) {
        ESP_LOGE(TAG, "Failed to query HA refresh tokens");
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
        return;
    }

    const cJSON *token = NULL;
    cJSON_ArrayForEach(token, result) {
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(token, "type");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(token,
                                                             "client_name");
        if (!cJSON_IsString(type) || !type->valuestring ||
            strcmp(type->valuestring, "long_lived_access_token") != 0 ||
            !cJSON_IsString(name) || !name->valuestring ||
            strcmp(name->valuestring, context->client_name) != 0) {
            continue;
        }

        const cJSON *token_id = cJSON_GetObjectItemCaseSensitive(token, "id");
        if (!cJSON_IsString(token_id) || !token_id->valuestring ||
            token_id->valuestring[0] == '\0' ||
            context->stale_tokens_deleted >=
                HA_AUTH_MAX_STALE_TOKEN_DELETIONS) {
            ESP_LOGE(TAG, "Invalid or excessive duplicate HA token entries");
            xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
            return;
        }

        cJSON *command = cJSON_CreateObject();
        if (!command) {
            xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
            return;
        }
        cJSON_AddStringToObject(command,
                                "type",
                                "auth/delete_refresh_token");
        cJSON_AddStringToObject(command,
                                "refresh_token_id",
                                token_id->valuestring);
        context->stale_tokens_deleted++;
        ESP_LOGI(TAG, "Deleting stale HA token named %s",
                 context->client_name);
        err = ha_ws_send_command(client,
                                 command,
                                 ha_auth_delete_stale_token_result_cb,
                                 context,
                                 NULL);
        cJSON_Delete(command);
        if (err != ESP_OK) {
            xEventGroupSetBits(context->events,
                               HA_AUTH_WORKER_EVENT_FAILED);
        }
        return;
    }

    err = ha_auth_send_long_lived_token_create(client, context);
    if (err != ESP_OK) {
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
    }
}

static void ha_auth_long_lived_connection_cb(ha_ws_client_t *client, ha_ws_event_t event, void *user_ctx)
{
    ha_long_lived_token_context_t *context = user_ctx;
    if (event == HA_WS_EVENT_AUTHENTICATED && !context->command_sent) {
        context->command_sent = true;
        esp_err_t err = ha_auth_build_long_lived_client_name(
            context->client_name,
            sizeof(context->client_name));
        if (err != ESP_OK) {
            xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
            return;
        }

        err = ha_auth_send_refresh_tokens_query(client, context);
        if (err != ESP_OK) {
            xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
        }
    }
    else if (event == HA_WS_EVENT_AUTH_INVALID || event == HA_WS_EVENT_ERROR ||
             event == HA_WS_EVENT_DISCONNECTED) {
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
    }
}

static esp_err_t ha_auth_create_long_lived_token(const char *hass_url,
                                                 const char *access_token,
                                                 char *long_lived_token,
                                                 size_t token_size)
{
    ha_long_lived_token_context_t context = {0};
    context.events = xEventGroupCreate();
    if (!context.events) {
        return ESP_ERR_NO_MEM;
    }

    ha_ws_client_t *client = NULL;
    ha_ws_config_t config = {
        .hass_url = hass_url,
        .access_token = access_token,
        .auto_reconnect = false,
        .coalesce_messages = false,
        .connection_cb = ha_auth_long_lived_connection_cb,
        .user_ctx = &context,
    };

    esp_err_t ret = ha_ws_client_init(&config, &client);
    if (ret == ESP_OK) {
        ret = ha_ws_connect(client);
    }
    if (ret == ESP_OK) {
        EventBits_t bits = xEventGroupWaitBits(context.events,
                                               HA_AUTH_WORKER_EVENT_SUCCESS | HA_AUTH_WORKER_EVENT_FAILED,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(HA_AUTH_WS_TIMEOUT_MS));
        if ((bits & HA_AUTH_WORKER_EVENT_SUCCESS) && context.long_lived_token[0] != '\0') {
            strlcpy(long_lived_token, context.long_lived_token, token_size);
            ret = ESP_OK;
        }
        else {
            ret = ESP_FAIL;
        }
    }

    if (client) {
        ha_ws_close(client);
        ha_ws_destroy(client);
    }
    vEventGroupDelete(context.events);
    ha_auth_secure_zero(context.long_lived_token, sizeof(context.long_lived_token));
    return ret;
}

static void ha_auth_verify_connection_cb(ha_ws_client_t *client, ha_ws_event_t event, void *user_ctx)
{
    (void)client;
    ha_verify_context_t *context = user_ctx;
    if (context->terminal) {
        return;
    }
    if (event == HA_WS_EVENT_AUTHENTICATED) {
        context->terminal = true;
        context->reason = HA_AUTH_REASON_NONE;
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_SUCCESS);
    }
    else if (event == HA_WS_EVENT_AUTH_INVALID) {
        context->terminal = true;
        context->reason = HA_AUTH_REASON_AUTH_INVALID;
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
    }
    else if (event == HA_WS_EVENT_ERROR || event == HA_WS_EVENT_DISCONNECTED) {
        context->terminal = true;
        context->reason = HA_AUTH_REASON_CONNECTION_ERROR;
        xEventGroupSetBits(context->events, HA_AUTH_WORKER_EVENT_FAILED);
    }
}

static esp_err_t ha_auth_verify_token_detailed(const char *hass_url,
                                               const char *token,
                                               uint32_t slot_timeout_ms,
                                               uint32_t verify_timeout_ms,
                                               ha_auth_reason_t *reason)
{
    if (reason) {
        *reason = HA_AUTH_REASON_CONNECTION_ERROR;
    }
    ha_verify_context_t context = {
        .events = xEventGroupCreate(),
        .reason = HA_AUTH_REASON_CONNECTION_ERROR,
    };
    if (!context.events) {
        return ESP_ERR_NO_MEM;
    }

    ha_ws_client_t *client = NULL;
    ha_ws_config_t config = {
        .hass_url = hass_url,
        .access_token = token,
        .auto_reconnect = false,
        .coalesce_messages = false,
        .connection_cb = ha_auth_verify_connection_cb,
        .user_ctx = &context,
    };
    esp_err_t ret = ha_ws_client_init(&config, &client);
    if (ret == ESP_OK) {
        ret = ha_ws_connect_timeout(client, slot_timeout_ms);
    }
    if (ret == ESP_OK) {
        EventBits_t bits = xEventGroupWaitBits(context.events,
                                               HA_AUTH_WORKER_EVENT_SUCCESS | HA_AUTH_WORKER_EVENT_FAILED,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(verify_timeout_ms));
        if (bits & HA_AUTH_WORKER_EVENT_SUCCESS) {
            ret = ESP_OK;
        }
        else if (bits & HA_AUTH_WORKER_EVENT_FAILED) {
            ret = ESP_FAIL;
        }
        else {
            ret = ESP_ERR_TIMEOUT;
        }
    }
    if (client) {
        ha_ws_close(client);
        ha_ws_destroy(client);
    }
    if (reason) {
        *reason = ret == ESP_OK ? HA_AUTH_REASON_NONE : context.reason;
    }
    vEventGroupDelete(context.events);
    return ret;
}

static esp_err_t ha_auth_verify_token(const char *hass_url, const char *token)
{
    return ha_auth_verify_token_detailed(hass_url,
                                         token,
                                         HA_AUTH_WS_TIMEOUT_MS,
                                         HA_AUTH_WS_TIMEOUT_MS,
                                         NULL);
}

static esp_err_t ha_auth_save_config(const char *hass_url,
                                     const char *token,
                                     uint32_t token_time,
                                     bool enabled)
{
    const nvs_storage_write_item_t items[] = {
        {.key = HA_AUTH_NVS_KEY_URL, .type = NVS_STORAGE_TYPE_STRING, .value.string = hass_url},
        {.key = HA_AUTH_NVS_KEY_TOKEN, .type = NVS_STORAGE_TYPE_STRING, .value.string = token},
        {.key = HA_AUTH_NVS_KEY_TOKEN_TIME, .type = NVS_STORAGE_TYPE_U32, .value.u32 = token_time},
        {.key = HA_AUTH_NVS_KEY_ENABLED, .type = NVS_STORAGE_TYPE_U8, .value.u8 = enabled ? 1 : 0},
    };
    return nvs_storage_write(HA_AUTH_NVS_NAMESPACE, items, sizeof(items) / sizeof(items[0]));
}

static void ha_auth_finish_update_url_worker(ha_update_url_worker_args_t *args,
                                             esp_err_t err,
                                             ha_auth_reason_t reason,
                                             bool changed)
{
    bool factory_resetting = false;
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    s_ha_auth.worker_running = false;
    factory_resetting = s_ha_auth.factory_resetting;
    xSemaphoreGive(s_ha_auth.lock);

    if (err != ESP_OK && !factory_resetting) {
        ha_auth_publish_status(args->enabled ? HA_AUTH_STATUS_CONNECTING
                                             : HA_AUTH_STATUS_DISABLED);
        ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    }

    ha_auth_connection_info_t info = {0};
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    ha_auth_copy_info_locked(&info);
    xSemaphoreGive(s_ha_auth.lock);

    if (args->callback) {
        args->callback(factory_resetting ? ESP_ERR_INVALID_STATE : err,
                       factory_resetting ? HA_AUTH_REASON_OAUTH_BUSY : reason,
                       factory_resetting ? false : changed,
                       &info,
                       args->user_ctx);
    }
    ha_auth_secure_zero(info.long_lived_token, sizeof(info.long_lived_token));
    ha_auth_secure_zero(args, sizeof(*args));
    free(args);
}

static void ha_auth_update_url_worker_task(void *param)
{
    ha_update_url_worker_args_t *args = param;
    ha_auth_reason_t reason = HA_AUTH_REASON_CONNECTION_ERROR;
    bool changed = false;
    bool operation_locked = false;

    esp_err_t err = ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_SUSPEND_REQUESTED);
    if (err != ESP_OK) {
        goto done;
    }

    ESP_LOGI(TAG, "Verifying updated HA URL before saving: %s", args->hass_url);
    err = ha_auth_verify_token_detailed(args->hass_url,
                                        args->token,
                                        HA_AUTH_UPDATE_URL_SLOT_TIMEOUT_MS,
                                        HA_AUTH_UPDATE_URL_VERIFY_TIMEOUT_MS,
                                        &reason);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Updated HA URL verification failed: reason=%s error=%s",
                 ha_auth_reason_to_string(reason),
                 esp_err_to_name(err));
        goto done;
    }

    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    operation_locked = true;
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (s_ha_auth.factory_resetting) {
        xSemaphoreGive(s_ha_auth.lock);
        err = ESP_ERR_INVALID_STATE;
        reason = HA_AUTH_REASON_OAUTH_BUSY;
        goto done;
    }

    err = nvs_storage_set_string(HA_AUTH_NVS_NAMESPACE,
                                 HA_AUTH_NVS_KEY_URL,
                                 args->hass_url);
    if (err != ESP_OK) {
        xSemaphoreGive(s_ha_auth.lock);
        reason = HA_AUTH_REASON_STORAGE_FAILED;
        goto done;
    }

    s_ha_auth.info.device_connected = false;
    strlcpy(s_ha_auth.info.hass_url,
            args->hass_url,
            sizeof(s_ha_auth.info.hass_url));
    s_ha_auth.info.status = args->enabled ? HA_AUTH_STATUS_CONNECTING
                                          : HA_AUTH_STATUS_DISABLED;
    xSemaphoreGive(s_ha_auth.lock);

    changed = true;
    reason = HA_AUTH_REASON_NONE;
    ESP_LOGI(TAG, "Updated HA URL verified and saved: %s", args->hass_url);
    ha_auth_publish_status(args->enabled ? HA_AUTH_STATUS_CONNECTING
                                         : HA_AUTH_STATUS_DISABLED);
    ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);

done:
    if (!operation_locked) {
        xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
        operation_locked = true;
    }
    ha_auth_finish_update_url_worker(args, err, reason, changed);
    if (operation_locked) {
        xSemaphoreGive(s_ha_auth.operation_lock);
    }
    vTaskDelete(NULL);
}

static void ha_auth_finish_worker(ha_auth_worker_args_t *args,
                                  esp_err_t err,
                                  ha_auth_reason_t reason)
{
    ha_auth_connection_info_t info = {0};
    bool factory_resetting = false;
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    s_ha_auth.worker_running = false;
    factory_resetting = s_ha_auth.factory_resetting;
    ha_auth_copy_info_locked(&info);
    xSemaphoreGive(s_ha_auth.lock);

    if (err != ESP_OK && !factory_resetting) {
        ha_auth_status_t fallback_status = HA_AUTH_STATUS_ERROR;
        if (info.configured) {
            fallback_status = info.device_connected ? HA_AUTH_STATUS_CONNECTED :
                              (info.enabled ? HA_AUTH_STATUS_DISCONNECTED : HA_AUTH_STATUS_DISABLED);
        }
        ha_auth_publish_status(fallback_status);
        ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    }
    if (args->callback && !factory_resetting) {
        args->callback(err, reason, &info, args->user_ctx);
    }
    ha_auth_secure_zero(info.long_lived_token, sizeof(info.long_lived_token));
    ha_auth_secure_zero(args, sizeof(*args));
    free(args);
}

static void ha_auth_worker_task(void *param)
{
    ha_auth_worker_args_t *args = param;
    char access_token[HA_AUTH_TOKEN_MAX_LEN] = {0};
    char refresh_token[HA_AUTH_TOKEN_MAX_LEN] = {0};
    char long_lived_token[HA_AUTH_TOKEN_MAX_LEN] = {0};
    ha_auth_reason_t reason = HA_AUTH_REASON_CODE_EXCHANGE_FAILED;
    bool operation_locked = false;
    bool authorization_succeeded = false;

    esp_err_t err = ha_auth_exchange_code(args->hass_url,
                                          args->code,
                                          args->client_id,
                                          access_token,
                                          sizeof(access_token),
                                          refresh_token,
                                          sizeof(refresh_token));
    ha_auth_secure_zero(args->code, sizeof(args->code));
    if (err != ESP_OK) goto done;

    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    s_ha_auth.info.device_connected = false;
    xSemaphoreGive(s_ha_auth.lock);
    err = ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_SUSPEND_REQUESTED);
    if (err != ESP_OK) goto done;
    ha_auth_publish_status(HA_AUTH_STATUS_CREATING_LONG_LIVED_TOKEN);
    reason = HA_AUTH_REASON_LONG_LIVED_TOKEN_FAILED;
    err = ha_auth_create_long_lived_token(args->hass_url,
                                          access_token,
                                          long_lived_token,
                                          sizeof(long_lived_token));
    if (err != ESP_OK) goto done;

    reason = HA_AUTH_REASON_HA_WS_CONNECT_FAILED;
    err = ha_auth_verify_token(args->hass_url, long_lived_token);
    if (err != ESP_OK) goto done;

    time_t now = time(NULL);
    uint32_t generated_at = now >= (time_t)HA_AUTH_VALID_EPOCH_MIN ? (uint32_t)now : 0;
    reason = HA_AUTH_REASON_STORAGE_FAILED;
    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    operation_locked = true;
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (s_ha_auth.factory_resetting) {
        xSemaphoreGive(s_ha_auth.lock);
        err = ESP_ERR_INVALID_STATE;
        goto done;
    }
    err = ha_auth_save_config(args->hass_url, long_lived_token, generated_at, true);
    xSemaphoreGive(s_ha_auth.lock);
    if (err != ESP_OK) goto done;

    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (s_ha_auth.factory_resetting) {
        xSemaphoreGive(s_ha_auth.lock);
        err = ESP_ERR_INVALID_STATE;
        goto done;
    }
    s_ha_auth.info.configured = true;
    s_ha_auth.info.enabled = true;
    s_ha_auth.info.device_connected = false;
    s_ha_auth.info.token_generated_at = generated_at;
    strlcpy(s_ha_auth.info.hass_url, args->hass_url, sizeof(s_ha_auth.info.hass_url));
    strlcpy(s_ha_auth.info.long_lived_token, long_lived_token, sizeof(s_ha_auth.info.long_lived_token));
    xSemaphoreGive(s_ha_auth.lock);

    err = ESP_OK;
    reason = HA_AUTH_REASON_NONE;
    authorization_succeeded = true;

done:
    if (refresh_token[0] != '\0') {
        ha_auth_revoke_refresh_token(args->hass_url, refresh_token);
    }
    ha_auth_secure_zero(access_token, sizeof(access_token));
    ha_auth_secure_zero(refresh_token, sizeof(refresh_token));
    ha_auth_secure_zero(long_lived_token, sizeof(long_lived_token));
    if (authorization_succeeded) {
        /* Do not start the persistent HA connection while this 16 KiB OAuth
         * worker is still revoking its temporary refresh token. Publishing
         * only after that network operation has completed minimizes the
         * overlap between the OAuth, LVGL rebuild, Entity Store and WebSocket
         * task stacks. The application adds a short settling delay before it
         * reconciles the data connection. */
        ha_auth_post_event(HA_AUTH_EVENT_LONG_LIVED_TOKEN_CREATED);
        ha_auth_publish_status(HA_AUTH_STATUS_CONNECTING);
        ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    }
    ha_auth_finish_worker(args, err, reason);
    if (operation_locked) {
        xSemaphoreGive(s_ha_auth.operation_lock);
    }
    vTaskDelete(NULL);
}

void ha_auth_set_status_callback(ha_auth_status_cb_t callback, void *user_ctx)
{
    s_ha_auth.status_callback = callback;
    s_ha_auth.status_user_ctx = user_ctx;
}

esp_err_t ha_auth_init(void)
{
    if (s_ha_auth.lock) {
        return ESP_OK;
    }
    s_ha_auth.lock = xSemaphoreCreateMutex();
    if (!s_ha_auth.lock) {
        return ESP_ERR_NO_MEM;
    }
    s_ha_auth.operation_lock = xSemaphoreCreateMutex();
    if (!s_ha_auth.operation_lock) {
        vSemaphoreDelete(s_ha_auth.lock);
        s_ha_auth.lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    uint8_t enabled = 0;
    nvs_storage_read_item_t items[] = {
        {.key = HA_AUTH_NVS_KEY_URL, .type = NVS_STORAGE_TYPE_STRING, .value = s_ha_auth.info.hass_url,
         .value_size = sizeof(s_ha_auth.info.hass_url)},
        {.key = HA_AUTH_NVS_KEY_TOKEN, .type = NVS_STORAGE_TYPE_STRING, .value = s_ha_auth.info.long_lived_token,
         .value_size = sizeof(s_ha_auth.info.long_lived_token)},
        {.key = HA_AUTH_NVS_KEY_TOKEN_TIME, .type = NVS_STORAGE_TYPE_U32,
         .value = &s_ha_auth.info.token_generated_at},
        {.key = HA_AUTH_NVS_KEY_ENABLED, .type = NVS_STORAGE_TYPE_U8, .value = &enabled},
    };
    esp_err_t ret = nvs_storage_read(HA_AUTH_NVS_NAMESPACE, items, sizeof(items) / sizeof(items[0]));
    if (ret == ESP_OK && items[0].result == ESP_OK && items[1].result == ESP_OK &&
        items[2].result == ESP_OK && items[3].result == ESP_OK &&
        s_ha_auth.info.hass_url[0] != '\0' && s_ha_auth.info.long_lived_token[0] != '\0') {
        s_ha_auth.info.configured = true;
        s_ha_auth.info.enabled = enabled != 0;
        s_ha_auth.info.status = s_ha_auth.info.enabled ? HA_AUTH_STATUS_CONNECTING : HA_AUTH_STATUS_DISABLED;
    }
    else {
        memset(&s_ha_auth.info, 0, sizeof(s_ha_auth.info));
        s_ha_auth.info.status = HA_AUTH_STATUS_NOT_CONFIGURED;
    }

    ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    ha_auth_publish_status(s_ha_auth.info.status);
    return ESP_OK;
}

esp_err_t ha_auth_start_oauth(const char *hass_url,
                              const char *redirect_uri,
                              char *authorize_url,
                              size_t authorize_url_size,
                              uint32_t *expires_in,
                              ha_auth_reason_t *reason)
{
    if (reason) *reason = HA_AUTH_REASON_INVALID_PARAM;
    if (!s_ha_auth.lock || !authorize_url || authorize_url_size == 0 || !expires_in ||
        !ha_auth_validate_url(hass_url) || !ha_auth_validate_url(redirect_uri)) {
        if (reason) *reason = HA_AUTH_REASON_INVALID_URL;
        return ESP_ERR_INVALID_ARG;
    }

    char normalized_url[HA_AUTH_URL_MAX_LEN] = {0};
    ha_auth_normalize_url(hass_url, normalized_url, sizeof(normalized_url));

    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    int64_t now = esp_timer_get_time();
    if (s_ha_auth.oauth.pending && now >= s_ha_auth.oauth.expires_at_us) {
        memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
    }
    if (s_ha_auth.factory_resetting || s_ha_auth.worker_running) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        if (reason) *reason = HA_AUTH_REASON_OAUTH_BUSY;
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ha_auth.oauth.pending) {
        /* Navigating to Home Assistant disconnects the original WebSocket, so
         * the device cannot distinguish a normal OAuth redirect from the user
         * pressing the browser Back button. A fresh StartOAuth safely replaces
         * the pending session: its new random state invalidates every callback
         * belonging to the abandoned authorization page. */
        ESP_LOGI(TAG, "Replacing pending OAuth session with a new authorization request");
        memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
    }

    ha_auth_generate_state(s_ha_auth.oauth.state);
    strlcpy(s_ha_auth.oauth.hass_url, normalized_url, sizeof(s_ha_auth.oauth.hass_url));
    strlcpy(s_ha_auth.oauth.client_id, redirect_uri, sizeof(s_ha_auth.oauth.client_id));
    strlcpy(s_ha_auth.oauth.redirect_uri, redirect_uri, sizeof(s_ha_auth.oauth.redirect_uri));
    s_ha_auth.oauth.expires_at_us = now + (int64_t)HA_AUTH_OAUTH_TIMEOUT_SECONDS * 1000000;
    s_ha_auth.oauth.pending = true;

    char state[sizeof(s_ha_auth.oauth.state)];
    char saved_url[sizeof(s_ha_auth.oauth.hass_url)];
    char client_id[sizeof(s_ha_auth.oauth.client_id)];
    char saved_redirect[sizeof(s_ha_auth.oauth.redirect_uri)];
    strlcpy(state, s_ha_auth.oauth.state, sizeof(state));
    strlcpy(saved_url, s_ha_auth.oauth.hass_url, sizeof(saved_url));
    strlcpy(client_id, s_ha_auth.oauth.client_id, sizeof(client_id));
    strlcpy(saved_redirect, s_ha_auth.oauth.redirect_uri, sizeof(saved_redirect));
    xSemaphoreGive(s_ha_auth.lock);

    char *encoded_client_id = ha_auth_url_encode(client_id);
    char *encoded_redirect = ha_auth_url_encode(saved_redirect);
    char *encoded_state = ha_auth_url_encode(state);
    if (!encoded_client_id || !encoded_redirect || !encoded_state) {
        free(encoded_client_id);
        free(encoded_redirect);
        free(encoded_state);
        xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
        memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        return ESP_ERR_NO_MEM;
    }

    int written = snprintf(authorize_url,
                           authorize_url_size,
                           "%s/auth/authorize?response_type=code&client_id=%s&redirect_uri=%s&state=%s",
                           saved_url,
                           encoded_client_id,
                           encoded_redirect,
                           encoded_state);
    free(encoded_client_id);
    free(encoded_redirect);
    free(encoded_state);
    ha_auth_secure_zero(state, sizeof(state));
    if (written < 0 || (size_t)written >= authorize_url_size) {
        xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
        memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        return ESP_ERR_INVALID_SIZE;
    }

    *expires_in = HA_AUTH_OAUTH_TIMEOUT_SECONDS;
    if (reason) *reason = HA_AUTH_REASON_NONE;
    ha_auth_publish_status(HA_AUTH_STATUS_AUTHORIZING);
    xSemaphoreGive(s_ha_auth.operation_lock);
    return ESP_OK;
}

esp_err_t ha_auth_complete_oauth(const char *code,
                                 const char *state,
                                 ha_auth_authorization_cb_t callback,
                                 void *user_ctx,
                                 ha_auth_reason_t *reason)
{
    if (reason) *reason = HA_AUTH_REASON_INVALID_PARAM;
    if (!s_ha_auth.lock || !code || !state || !callback || code[0] == '\0' || state[0] == '\0' ||
        strlen(code) >= HA_AUTH_CODE_MAX_LEN || strlen(state) != HA_AUTH_STATE_HEX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    ha_auth_worker_args_t *args = calloc(1, sizeof(*args));
    if (!args) return ESP_ERR_NO_MEM;

    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (!s_ha_auth.oauth.pending || s_ha_auth.worker_running) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        free(args);
        if (reason) *reason = HA_AUTH_REASON_OAUTH_BUSY;
        return ESP_ERR_INVALID_STATE;
    }
    if (esp_timer_get_time() >= s_ha_auth.oauth.expires_at_us) {
        memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        free(args);
        if (reason) *reason = HA_AUTH_REASON_STATE_EXPIRED;
        return ESP_ERR_TIMEOUT;
    }
    if (!ha_auth_constant_time_equal(state, s_ha_auth.oauth.state)) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        free(args);
        if (reason) *reason = HA_AUTH_REASON_STATE_MISMATCH;
        return ESP_ERR_INVALID_CRC;
    }

    strlcpy(args->hass_url, s_ha_auth.oauth.hass_url, sizeof(args->hass_url));
    strlcpy(args->client_id, s_ha_auth.oauth.client_id, sizeof(args->client_id));
    strlcpy(args->code, code, sizeof(args->code));
    args->callback = callback;
    args->user_ctx = user_ctx;
    memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
    s_ha_auth.worker_running = true;
    xSemaphoreGive(s_ha_auth.lock);

    BaseType_t task_result = xTaskCreate(ha_auth_worker_task,
                                         "ha_oauth",
                                         HA_AUTH_WORKER_STACK_SIZE,
                                         args,
                                         7,
                                         NULL);
    if (task_result != pdPASS) {
        xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
        s_ha_auth.worker_running = false;
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        ha_auth_secure_zero(args, sizeof(*args));
        free(args);
        return ESP_ERR_NO_MEM;
    }

    if (reason) *reason = HA_AUTH_REASON_NONE;
    ha_auth_publish_status(HA_AUTH_STATUS_EXCHANGING_CODE);
    xSemaphoreGive(s_ha_auth.operation_lock);
    return ESP_OK;
}

esp_err_t ha_auth_get_connection_info(ha_auth_connection_info_t *info)
{
    if (!s_ha_auth.lock || !info) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    ha_auth_copy_info_locked(info);
    xSemaphoreGive(s_ha_auth.lock);
    return ESP_OK;
}

void ha_auth_handle_ws_event(ha_ws_event_t event)
{
    if (!s_ha_auth.lock) {
        return;
    }

    ha_auth_status_t status;
    ha_auth_reason_t reason = HA_AUTH_REASON_NONE;
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (!s_ha_auth.info.configured || !s_ha_auth.info.enabled) {
        xSemaphoreGive(s_ha_auth.lock);
        return;
    }

    status = s_ha_auth.info.status;
    switch (event) {
    case HA_WS_EVENT_CONNECTED:
    case HA_WS_EVENT_AUTH_REQUIRED:
        s_ha_auth.info.device_connected = false;
        status = HA_AUTH_STATUS_CONNECTING;
        break;
    case HA_WS_EVENT_AUTHENTICATED:
        s_ha_auth.info.device_connected = true;
        status = HA_AUTH_STATUS_CONNECTED;
        break;
    case HA_WS_EVENT_AUTH_INVALID:
        s_ha_auth.info.device_connected = false;
        status = HA_AUTH_STATUS_ERROR;
        reason = HA_AUTH_REASON_AUTH_INVALID;
        break;
    case HA_WS_EVENT_ERROR:
        s_ha_auth.info.device_connected = false;
        status = HA_AUTH_STATUS_ERROR;
        reason = s_ha_auth.info.reason == HA_AUTH_REASON_AUTH_INVALID
                     ? HA_AUTH_REASON_AUTH_INVALID
                     : HA_AUTH_REASON_CONNECTION_ERROR;
        break;
    case HA_WS_EVENT_DISCONNECTED:
        s_ha_auth.info.device_connected = false;
        if (s_ha_auth.info.reason == HA_AUTH_REASON_AUTH_INVALID) {
            status = HA_AUTH_STATUS_ERROR;
            reason = HA_AUTH_REASON_AUTH_INVALID;
        }
        else {
            status = HA_AUTH_STATUS_DISCONNECTED;
        }
        break;
    default:
        xSemaphoreGive(s_ha_auth.lock);
        return;
    }
    xSemaphoreGive(s_ha_auth.lock);

    ha_auth_publish_status_with_reason(status, reason);
}

esp_err_t ha_auth_update_url(const char *hass_url,
                             ha_auth_update_url_cb_t callback,
                             void *user_ctx,
                             ha_auth_reason_t *reason)
{
    if (reason) *reason = HA_AUTH_REASON_INVALID_PARAM;
    if (!s_ha_auth.lock || !s_ha_auth.operation_lock || !callback) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ha_auth_validate_url(hass_url)) {
        if (reason) *reason = HA_AUTH_REASON_INVALID_URL;
        return ESP_ERR_INVALID_ARG;
    }

    char normalized_url[HA_AUTH_URL_MAX_LEN] = {0};
    ha_auth_normalize_url(hass_url, normalized_url, sizeof(normalized_url));
    ha_update_url_worker_args_t *args = calloc(1, sizeof(*args));
    if (!args) {
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    int64_t now = esp_timer_get_time();
    if (s_ha_auth.oauth.pending && now >= s_ha_auth.oauth.expires_at_us) {
        memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
    }
    if (s_ha_auth.factory_resetting || s_ha_auth.oauth.pending || s_ha_auth.worker_running) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        free(args);
        if (reason) *reason = HA_AUTH_REASON_OAUTH_BUSY;
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_ha_auth.info.configured) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        free(args);
        if (reason) *reason = HA_AUTH_REASON_NOT_CONFIGURED;
        return ESP_ERR_NOT_FOUND;
    }
    if (strcmp(normalized_url, s_ha_auth.info.hass_url) == 0) {
        ha_auth_connection_info_t info = {0};
        ha_auth_copy_info_locked(&info);
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        if (reason) *reason = HA_AUTH_REASON_NONE;
        callback(ESP_OK, HA_AUTH_REASON_NONE, false, &info, user_ctx);
        ha_auth_secure_zero(info.long_lived_token, sizeof(info.long_lived_token));
        free(args);
        return ESP_OK;
    }

    strlcpy(args->hass_url, normalized_url, sizeof(args->hass_url));
    strlcpy(args->token,
            s_ha_auth.info.long_lived_token,
            sizeof(args->token));
    args->enabled = s_ha_auth.info.enabled;
    args->callback = callback;
    args->user_ctx = user_ctx;
    s_ha_auth.worker_running = true;
    xSemaphoreGive(s_ha_auth.lock);

    BaseType_t task_result = xTaskCreate(ha_auth_update_url_worker_task,
                                         "ha_url_verify",
                                         HA_AUTH_UPDATE_URL_WORKER_STACK_SIZE,
                                         args,
                                         7,
                                         NULL);
    if (task_result != pdPASS) {
        xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
        s_ha_auth.worker_running = false;
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        ha_auth_secure_zero(args, sizeof(*args));
        free(args);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(s_ha_auth.operation_lock);
    if (reason) *reason = HA_AUTH_REASON_NONE;
    return ESP_OK;
}

esp_err_t ha_auth_activate(ha_auth_reason_t *reason)
{
    if (reason) *reason = HA_AUTH_REASON_NONE;
    if (!s_ha_auth.lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (s_ha_auth.factory_resetting || s_ha_auth.worker_running) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        if (reason) *reason = HA_AUTH_REASON_OAUTH_BUSY;
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_ha_auth.info.configured) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        if (reason) *reason = HA_AUTH_REASON_NOT_CONFIGURED;
        return ESP_ERR_NOT_FOUND;
    }
    bool already_enabled = s_ha_auth.info.enabled;
    if (!already_enabled) {
        esp_err_t err = nvs_storage_set_u8(HA_AUTH_NVS_NAMESPACE, HA_AUTH_NVS_KEY_ENABLED, 1);
        if (err != ESP_OK) {
            xSemaphoreGive(s_ha_auth.lock);
            xSemaphoreGive(s_ha_auth.operation_lock);
            if (reason) *reason = HA_AUTH_REASON_STORAGE_FAILED;
            return err;
        }
        s_ha_auth.info.enabled = true;
        s_ha_auth.info.device_connected = false;
    }
    xSemaphoreGive(s_ha_auth.lock);

    if (!already_enabled) {
        ha_auth_publish_status(HA_AUTH_STATUS_CONNECTING);
        ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    }

    xSemaphoreGive(s_ha_auth.operation_lock);
    return ESP_OK;
}

esp_err_t ha_auth_disable(void)
{
    if (!s_ha_auth.lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (s_ha_auth.factory_resetting || s_ha_auth.worker_running) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        return ESP_ERR_INVALID_STATE;
    }
    bool configured = s_ha_auth.info.configured;
    if (configured) {
        esp_err_t err = nvs_storage_set_u8(HA_AUTH_NVS_NAMESPACE, HA_AUTH_NVS_KEY_ENABLED, 0);
        if (err != ESP_OK) {
            xSemaphoreGive(s_ha_auth.lock);
            xSemaphoreGive(s_ha_auth.operation_lock);
            return err;
        }
    }
    s_ha_auth.info.enabled = false;
    s_ha_auth.info.device_connected = false;
    xSemaphoreGive(s_ha_auth.lock);

    ha_auth_publish_status(configured ? HA_AUTH_STATUS_DISABLED : HA_AUTH_STATUS_NOT_CONFIGURED);
    ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    xSemaphoreGive(s_ha_auth.operation_lock);
    return ESP_OK;
}

esp_err_t ha_auth_remove_authorization(void)
{
    if (!s_ha_auth.lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    if (s_ha_auth.factory_resetting || s_ha_auth.worker_running) {
        xSemaphoreGive(s_ha_auth.lock);
        xSemaphoreGive(s_ha_auth.operation_lock);
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
    xSemaphoreGive(s_ha_auth.lock);
    esp_err_t err = nvs_storage_erase_namespace(HA_AUTH_NVS_NAMESPACE);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        xSemaphoreGive(s_ha_auth.operation_lock);
        return err;
    }

    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    ha_auth_secure_zero(&s_ha_auth.info, sizeof(s_ha_auth.info));
    ha_auth_secure_zero(&s_ha_auth.oauth, sizeof(s_ha_auth.oauth));
    s_ha_auth.info.status = HA_AUTH_STATUS_NOT_CONFIGURED;
    xSemaphoreGive(s_ha_auth.lock);
    ha_auth_publish_status(HA_AUTH_STATUS_NOT_CONFIGURED);
    ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    xSemaphoreGive(s_ha_auth.operation_lock);
    return ESP_OK;
}

esp_err_t ha_auth_prepare_factory_reset(void)
{
    if (!s_ha_auth.lock) {
        esp_err_t err = nvs_storage_erase_namespace(HA_AUTH_NVS_NAMESPACE);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
    }

    xSemaphoreTake(s_ha_auth.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    s_ha_auth.factory_resetting = true;
    memset(&s_ha_auth.oauth, 0, sizeof(s_ha_auth.oauth));
    s_ha_auth.info.enabled = false;
    xSemaphoreGive(s_ha_auth.lock);

    esp_err_t err = nvs_storage_erase_namespace(HA_AUTH_NVS_NAMESPACE);

    xSemaphoreTake(s_ha_auth.lock, portMAX_DELAY);
    ha_auth_secure_zero(&s_ha_auth.info, sizeof(s_ha_auth.info));
    s_ha_auth.info.status = HA_AUTH_STATUS_NOT_CONFIGURED;
    xSemaphoreGive(s_ha_auth.lock);
    ha_auth_post_event(HA_AUTH_EVENT_CONNECTION_INFO_CHANGED);
    xSemaphoreGive(s_ha_auth.operation_lock);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}
