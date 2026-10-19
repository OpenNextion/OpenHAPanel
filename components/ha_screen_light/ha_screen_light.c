#include "ha_screen_light.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "bsp_lcd.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ha_auth.h"
#include "ha_ws.h"
#include "nvs_storage.h"
#include "screen_night_mode.h"
#include "webserver_config.h"
#include "webserver_config_api.h"
#include "webserver_screen.h"

#define EVENT_TYPE "nextion_screen_light_command"
#define HTTP_RESPONSE_LIMIT 16384
#define WORKER_STACK_SIZE 10240
#define STORAGE_NAMESPACE "ha_scr_light"

static const char *TAG = "ha_screen_light";

typedef enum { MESSAGE_RESULT, MESSAGE_COMMAND } message_kind_t;
typedef enum { COMMAND_ON, COMMAND_OFF, COMMAND_LEVEL } command_kind_t;

typedef struct {
    message_kind_t kind;
    int id;
    esp_err_t err;
    cJSON *result;
    command_kind_t command;
    int brightness;
} message_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool overflow;
} http_response_t;

typedef struct {
    QueueHandle_t queue;
    QueueHandle_t command_queue;
    SemaphoreHandle_t done;
    TaskHandle_t task;
    ha_ws_client_t *client;
    volatile bool stop;
    bool ready;
    char mac[13];
    char suffix[5];
    char suffix_lower[5];
    char lock_id[64];
    char brightness_id[64];
    char light_id[64];
    char lock_name[64];
    char brightness_name[64];
    char light_name[64];
    int last_brightness;
    int last_locked;
} screen_light_t;

static screen_light_t s_light;

static const char *json_string(const cJSON *object, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static bool json_is(const cJSON *object, const char *key, const char *value)
{
    const char *actual = json_string(object, key);
    return actual && strcmp(actual, value) == 0;
}

static void result_cb(ha_ws_client_t *client, int id, esp_err_t err,
                      const cJSON *result, const cJSON *error, void *user_ctx)
{
    (void)client;
    screen_light_t *state = user_ctx;
    message_t msg = {.kind = MESSAGE_RESULT, .id = id, .err = err};
    msg.result = cJSON_Duplicate(err == ESP_OK ? result : error, true);
    if (xQueueSend(state->queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "WebSocket result queue full, request id=%d", id);
        cJSON_Delete(msg.result);
    }
}

static esp_err_t ws_request(screen_light_t *state, cJSON *request, cJSON **out)
{
    const char *type = json_string(request, "type");
    char type_name[48];
    snprintf(type_name, sizeof(type_name), "%.47s", type ? type : "unknown");
    ESP_LOGI(TAG, "HA WS request: %s", type_name);
    int request_id = 0;
    esp_err_t err = ha_ws_send_command(state->client, request, result_cb, state,
                                       &request_id);
    cJSON_Delete(request);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HA WS send failed: %s", esp_err_to_name(err));
        return err;
    }
    for (int waited = 0; waited < 10000 && !state->stop; waited += 250) {
        message_t msg;
        if (xQueueReceive(state->queue, &msg, pdMS_TO_TICKS(250)) != pdTRUE) {
            continue;
        }
        if (msg.kind != MESSAGE_RESULT || msg.id != request_id) {
            cJSON_Delete(msg.result);
            continue;
        }
        if (msg.err != ESP_OK) {
            char *detail = cJSON_PrintUnformatted(msg.result);
            ESP_LOGE(TAG, "HA WebSocket command %d failed: %s", request_id,
                     detail ? detail : "unknown error");
            cJSON_free(detail);
        }
        bool permission_denied = msg.err != ESP_OK &&
                                 json_is(msg.result, "code", "unauthorized");
        ESP_LOGI(TAG, "HA WS result: %s id=%d status=%s", type_name,
                 request_id, esp_err_to_name(msg.err));
        *out = msg.result;
        return permission_denied ? ESP_ERR_NOT_ALLOWED : msg.err;
    }
    ESP_LOGW(TAG, "HA WS request timed out: %s id=%d", type_name, request_id);
    return ESP_ERR_TIMEOUT;
}

static cJSON *new_ws_command(const char *type)
{
    cJSON *request = cJSON_CreateObject();
    if (request && cJSON_AddStringToObject(request, "type", type)) {
        return request;
    }
    cJSON_Delete(request);
    return NULL;
}

static esp_err_t get_registry_entries(screen_light_t *state,
                                       const char *const *entity_ids,
                                       size_t count, cJSON **out)
{
    cJSON *request = new_ws_command("config/entity_registry/get_entries");
    cJSON *ids = cJSON_CreateArray();
    if (!request || !ids) {
        cJSON_Delete(request);
        cJSON_Delete(ids);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < count; i++) {
        cJSON *id = cJSON_CreateString(entity_ids[i]);
        if (!id) {
            cJSON_Delete(request);
            cJSON_Delete(ids);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddItemToArray(ids, id);
    }
    cJSON_AddItemToObject(request, "entity_ids", ids);
    esp_err_t err = ws_request(state, request, out);
    if (err == ESP_OK && !cJSON_IsObject(*out)) {
        cJSON_Delete(*out);
        *out = NULL;
        return ESP_FAIL;
    }
    return err;
}

static esp_err_t ensure_helper(screen_light_t *state, const char *domain,
                               const char *name, const char *entity_id,
                               const cJSON *entry)
{
    const char *expected_id = strchr(entity_id, '.');
    expected_id = expected_id ? expected_id + 1 : entity_id;
    if (cJSON_IsObject(entry)) {
        bool owned = json_is(entry, "platform", domain) &&
                     json_is(entry, "unique_id", expected_id);
        if (!owned) {
            ESP_LOGE(TAG, "%s is occupied by a different entity", entity_id);
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGI(TAG, "Helper already exists: %s", entity_id);
        return ESP_OK;
    }
    if (!cJSON_IsNull(entry)) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Creating helper: %s", entity_id);
    char type[40];
    snprintf(type, sizeof(type), "%s/create", domain);
    cJSON *request = new_ws_command(type);
    if (!request || !cJSON_AddStringToObject(request, "name", name)) {
        cJSON_Delete(request);
        return ESP_ERR_NO_MEM;
    }
    if (strcmp(domain, "input_number") == 0) {
        cJSON_AddNumberToObject(request, "min", 0);
        cJSON_AddNumberToObject(request, "max", 100);
        cJSON_AddNumberToObject(request, "step", 1);
        cJSON_AddStringToObject(request, "mode", "slider");
    }
    cJSON *created = NULL;
    esp_err_t err = ws_request(state, request, &created);
    if (err == ESP_OK && !json_is(created, "id", expected_id)) {
        ESP_LOGE(TAG, "Helper %s received an unexpected storage ID", entity_id);
        err = ESP_ERR_INVALID_STATE;
    }
    cJSON_Delete(created);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Created %s", entity_id);
    }
    return err;
}

static esp_err_t check_light_registry(screen_light_t *state, bool *exists)
{
    *exists = false;
    const char *ids[] = {state->light_id};
    cJSON *entries = NULL;
    esp_err_t err = get_registry_entries(state, ids, 1, &entries);
    if (err != ESP_OK) {
        return err;
    }
    const cJSON *light = cJSON_GetObjectItemCaseSensitive(entries, state->light_id);
    if (cJSON_IsObject(light)) {
        if (!json_is(light, "platform", "template")) {
            ESP_LOGE(TAG, "%s belongs to another integration", state->light_id);
            err = ESP_ERR_INVALID_STATE;
        } else {
            *exists = true;
            ESP_LOGI(TAG, "Template Light already exists: %s", state->light_id);
        }
    } else if (!cJSON_IsNull(light)) {
        err = ESP_FAIL;
    }
    cJSON_Delete(entries);
    return err;
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    http_response_t *response = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
        return ESP_OK;
    }
    if (response->length + event->data_len >= response->capacity) {
        response->overflow = true;
        return ESP_OK;
    }
    memcpy(response->data + response->length, event->data, event->data_len);
    response->length += event->data_len;
    response->data[response->length] = '\0';
    return ESP_OK;
}

static esp_err_t http_json(const ha_auth_connection_info_t *info,
                           const char *path, cJSON *body, cJSON **out)
{
    size_t base_len = strlen(info->hass_url);
    char url[HA_AUTH_URL_MAX_LEN + 160];
    snprintf(url, sizeof(url), "%s%s%s", info->hass_url,
             base_len && info->hass_url[base_len - 1] == '/' ? "" : "/", path);
    char *buffer = calloc(1, HTTP_RESPONSE_LIMIT);
    char *payload = body ? cJSON_PrintUnformatted(body) : NULL;
    if (!buffer || (body && !payload)) {
        free(buffer);
        cJSON_free(payload);
        return ESP_ERR_NO_MEM;
    }
    http_response_t response = {.data = buffer, .capacity = HTTP_RESPONSE_LIMIT};
    esp_http_client_config_t config = {
        .url = url, .event_handler = http_event, .user_data = &response,
        .timeout_ms = 10000, .disable_auto_redirect = true,
    };
    esp_http_client_handle_t http = esp_http_client_init(&config);
    if (!http) {
        free(buffer);
        cJSON_free(payload);
        return ESP_FAIL;
    }
    char authorization[HA_AUTH_TOKEN_MAX_LEN + 16];
    snprintf(authorization, sizeof(authorization), "Bearer %s", info->long_lived_token);
    esp_http_client_set_header(http, "Authorization", authorization);
    esp_http_client_set_header(http, "Content-Type", "application/json");
    esp_http_client_set_header(http, "Accept", "application/json");
    if (body) {
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        esp_http_client_set_post_field(http, payload, strlen(payload));
    }
    ESP_LOGI(TAG, "HA HTTP %s %s", body ? "POST" : "GET", path);
    esp_err_t err = esp_http_client_perform(http);
    int status = esp_http_client_get_status_code(http);
    esp_http_client_cleanup(http);
    memset(authorization, 0, sizeof(authorization));
    cJSON_free(payload);
    if (err == ESP_OK && (status < 200 || status >= 300 || response.overflow)) {
        ESP_LOGE(TAG, "HA HTTP %s returned status %d%s", path, status,
                 response.overflow ? " (response too large)" : "");
        err = status == 401 || status == 403 ? ESP_ERR_NOT_ALLOWED : ESP_FAIL;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HA HTTP request failed: %s (%s)", path, esp_err_to_name(err));
    }
    if (err == ESP_OK) {
        *out = cJSON_Parse(buffer);
        if (!*out) {
            ESP_LOGE(TAG, "HA HTTP response is not JSON: %s", path);
            err = ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "HA HTTP response: %s status=%d type=%s step=%s", path,
                     status, json_string(*out, "type") ? json_string(*out, "type") : "-",
                     json_string(*out, "step_id") ? json_string(*out, "step_id") : "-");
        }
    }
    free(buffer);
    return err;
}

static cJSON *make_event_action(const screen_light_t *state,
                                const char *command, bool with_level)
{
    cJSON *actions = cJSON_CreateArray();
    cJSON *action = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if (!actions || !action || !data) {
        cJSON_Delete(actions);
        cJSON_Delete(action);
        cJSON_Delete(data);
        return NULL;
    }
    cJSON_AddStringToObject(action, "event", EVENT_TYPE);
    cJSON_AddStringToObject(data, "device_id", state->mac);
    cJSON_AddStringToObject(data, "action", command);
    if (with_level) {
        cJSON_AddStringToObject(data, "brightness", "{{ brightness }}");
        cJSON_AddStringToObject(data, "brightness_pct", "{{ brightness_pct }}");
    }
    cJSON_AddItemToObject(action, "event_data", data);
    cJSON_AddItemToArray(actions, action);
    return actions;
}

static cJSON *make_light_form(const screen_light_t *state, bool include_name)
{
    char state_template[200];
    char level_template[180];
    snprintf(state_template, sizeof(state_template),
             "{{ is_state('%s', 'off') and (states('%s') | float(0)) > 0 }}",
             state->lock_id, state->brightness_id);
    snprintf(level_template, sizeof(level_template),
             "{{ ((states('%s') | float(0)) * 255 / 100) | round(0) | int }}",
             state->brightness_id);
    cJSON *body = cJSON_CreateObject();
    cJSON *turn_on = make_event_action(state, "on", false);
    cJSON *turn_off = make_event_action(state, "off", false);
    cJSON *set_level = make_event_action(state, "level", true);
    if (!body || !turn_on || !turn_off || !set_level) {
        cJSON_Delete(body);
        cJSON_Delete(turn_on);
        cJSON_Delete(turn_off);
        cJSON_Delete(set_level);
        return NULL;
    }
    if (include_name) {
        cJSON_AddStringToObject(body, "name", state->light_name);
    }
    cJSON_AddStringToObject(body, "state", state_template);
    cJSON_AddStringToObject(body, "level", level_template);
    cJSON_AddItemToObject(body, "turn_on", turn_on);
    cJSON_AddItemToObject(body, "turn_off", turn_off);
    cJSON_AddItemToObject(body, "set_level", set_level);
    return body;
}

static esp_err_t mark_light_seen(void)
{
    const nvs_storage_write_item_t item = {
        .key = "entity_seen", .type = NVS_STORAGE_TYPE_STRING,
        .value.string = "1",
    };
    return nvs_storage_write(STORAGE_NAMESPACE, &item, 1);
}

/* A config entry may exist even while its Light entity is unavailable. Wait for
 * entries still setting up; a loaded entry without the expected entity ID may
 * have been renamed, so a new flow may create the expected ID. */
static esp_err_t check_saved_light_entry(screen_light_t *state,
                                         const ha_auth_connection_info_t *info)
{
    char saved_url[HA_AUTH_URL_MAX_LEN] = {0};
    char entry_id[64] = {0};
    esp_err_t url_err = nvs_storage_get_string(STORAGE_NAMESPACE, "ha_url",
                                                saved_url, sizeof(saved_url));
    esp_err_t id_err = nvs_storage_get_string(STORAGE_NAMESPACE, "entry_id",
                                               entry_id, sizeof(entry_id));
    if (url_err == ESP_ERR_NVS_NOT_FOUND && id_err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved Template config entry");
        return ESP_OK;
    }
    if (url_err != ESP_OK || id_err != ESP_OK) {
        return url_err != ESP_OK ? url_err : id_err;
    }
    if (strcmp(saved_url, info->hass_url) != 0) {
        ESP_LOGI(TAG, "Saved Template entry belongs to another HA server");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Checking saved Template config entry %s", entry_id);
    cJSON *request = new_ws_command("config_entries/get_single");
    if (!request || !cJSON_AddStringToObject(request, "entry_id", entry_id)) {
        cJSON_Delete(request);
        return ESP_ERR_NO_MEM;
    }
    cJSON *result = NULL;
    esp_err_t err = ws_request(state, request, &result);
    if (err == ESP_OK) {
        const cJSON *config_entry = cJSON_GetObjectItemCaseSensitive(result,
                                                                      "config_entry");
        bool matching = json_is(config_entry, "domain", "template");
        bool loaded = json_is(config_entry, "state", "loaded");
        const char *entry_state = json_string(config_entry, "state");
        if (!matching) {
            ESP_LOGE(TAG, "Saved config entry %s is not a Template entry", entry_id);
            cJSON_Delete(result);
            return ESP_ERR_INVALID_STATE;
        }
        if (loaded) {
            char seen[4] = {0};
            if (nvs_storage_get_string(STORAGE_NAMESPACE, "entity_seen", seen,
                                       sizeof(seen)) == ESP_OK && strcmp(seen, "0") == 0) {
                ESP_LOGW(TAG, "New Template entry %s is loaded but %s has not appeared yet; waiting", entry_id, state->light_id);
                cJSON_Delete(result);
                return ESP_ERR_INVALID_STATE;
            }
            ESP_LOGW(TAG, "Template entry %s is loaded but %s is missing; its entity ID may have been renamed, creating a new Light", entry_id, state->light_id);
            cJSON_Delete(result);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "Template entry %s is %s; waiting for %s before creating another", entry_id,
                 entry_state ? entry_state : "unknown", state->light_id);
        cJSON_Delete(result);
        return ESP_ERR_INVALID_STATE;
    }
    bool deleted = json_is(result, "code", "not_found");
    cJSON_Delete(result);
    if (!deleted) {
        return err;
    }
    return nvs_storage_erase_namespace(STORAGE_NAMESPACE);
}

/* Upgrade only the Template entry created by this device. Entity registry
 * ownership is checked before using the options flow. */
static void upgrade_saved_light_action(screen_light_t *state,
                                       const ha_auth_connection_info_t *info,
                                       const cJSON *light)
{
    char version[8] = {0};
    if (nvs_storage_get_string(STORAGE_NAMESPACE, "action_schema", version,
                               sizeof(version)) == ESP_OK && strcmp(version, "2") == 0) {
        return;
    }
    char saved_url[HA_AUTH_URL_MAX_LEN] = {0};
    char entry_id[64] = {0};
    if (nvs_storage_get_string(STORAGE_NAMESPACE, "ha_url", saved_url,
                               sizeof(saved_url)) != ESP_OK ||
        nvs_storage_get_string(STORAGE_NAMESPACE, "entry_id", entry_id,
                               sizeof(entry_id)) != ESP_OK ||
        strcmp(saved_url, info->hass_url) != 0) {
        ESP_LOGW(TAG, "Existing Light has no matching saved entry; edit its set_level action in HA");
        return;
    }
    bool owned = json_is(light, "config_entry_id", entry_id) &&
                 json_is(light, "platform", "template");
    if (!owned) {
        ESP_LOGW(TAG, "Existing Light is not linked to saved Template entry; skipping action upgrade");
        return;
    }
    ESP_LOGI(TAG, "Upgrading brightness action for %s", state->light_id);
    cJSON *body = cJSON_CreateObject();
    if (!body || !cJSON_AddStringToObject(body, "handler", entry_id)) {
        cJSON_Delete(body);
        return;
    }
    cJSON *response = NULL;
    esp_err_t err = http_json(info, "api/config/config_entries/options/flow",
                              body, &response);
    cJSON_Delete(body);
    if (err != ESP_OK) {
        return;
    }
    const char *flow_id = json_string(response, "flow_id");
    char path[210];
    if (!flow_id) {
        ESP_LOGW(TAG, "Template options flow returned no flow_id");
        cJSON_Delete(response);
        return;
    }
    snprintf(path, sizeof(path), "api/config/config_entries/options/flow/%.80s", flow_id);
    bool form = json_is(response, "type", "form") &&
                json_is(response, "step_id", "light");
    cJSON_Delete(response);
    if (!form) {
        ESP_LOGW(TAG, "Template options flow did not return a light form");
        return;
    }
    body = make_light_form(state, false);
    if (!body) {
        return;
    }
    response = NULL;
    err = http_json(info, path, body, &response);
    cJSON_Delete(body);
    if (err != ESP_OK) {
        return;
    }
    bool updated = json_is(response, "type", "create_entry");
    cJSON_Delete(response);
    if (!updated) {
        ESP_LOGW(TAG, "Template options flow did not update the Light");
        return;
    }
    const nvs_storage_write_item_t marker = {
        .key = "action_schema", .type = NVS_STORAGE_TYPE_STRING,
        .value.string = "2",
    };
    err = nvs_storage_write(STORAGE_NAMESPACE, &marker, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Brightness action upgraded to 0-255 brightness variable");
    } else {
        ESP_LOGW(TAG, "Brightness action updated but NVS marker save failed: %s",
                 esp_err_to_name(err));
    }
}

static esp_err_t ensure_light(screen_light_t *state,
                              const ha_auth_connection_info_t *info,
                              const cJSON *light)
{
    if (cJSON_IsObject(light)) {
        if (!json_is(light, "platform", "template")) {
            ESP_LOGE(TAG, "%s belongs to another integration", state->light_id);
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGI(TAG, "Template Light already exists: %s", state->light_id);
        char seen[4] = {0};
        if (nvs_storage_get_string(STORAGE_NAMESPACE, "entity_seen", seen,
                                   sizeof(seen)) == ESP_OK && strcmp(seen, "0") == 0) {
            esp_err_t save_err = mark_light_seen();
            if (save_err != ESP_OK) {
                ESP_LOGW(TAG, "Could not record that Light appeared: %s",
                         esp_err_to_name(save_err));
            }
        }
        upgrade_saved_light_action(state, info, light);
        return ESP_OK;
    }
    if (!cJSON_IsNull(light)) {
        return ESP_FAIL;
    }
    esp_err_t err = check_saved_light_entry(state, info);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "handler", "template");
    cJSON *response = NULL;
    err = http_json(info, "api/config/config_entries/flow", body, &response);
    cJSON_Delete(body);
    if (err != ESP_OK) {
        return err;
    }
    const char *flow_id = json_string(response, "flow_id");
    if (!json_is(response, "type", "menu") || !flow_id) {
        ESP_LOGE(TAG, "Template flow did not return menu and flow_id");
        cJSON_Delete(response);
        return ESP_FAIL;
    }
    char path[200];
    snprintf(path, sizeof(path), "api/config/config_entries/flow/%s", flow_id);
    cJSON_Delete(response);
    body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "next_step_id", "light");
    err = http_json(info, path, body, &response);
    cJSON_Delete(body);
    if (err != ESP_OK) {
        return err;
    }
    if (!json_is(response, "type", "form") || !json_is(response, "step_id", "light")) {
        ESP_LOGE(TAG, "Template flow did not return the light form");
        cJSON_Delete(response);
        return ESP_FAIL;
    }
    cJSON_Delete(response);

    body = make_light_form(state, true);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }
    err = http_json(info, path, body, &response);
    cJSON_Delete(body);
    if (err != ESP_OK) {
        return err;
    }
    const cJSON *created_entry = cJSON_GetObjectItemCaseSensitive(response, "result");
    const char *created_id = json_string(created_entry, "entry_id");
    bool created = json_is(response, "type", "create_entry") && created_id;
    if (!created) {
        char *detail = cJSON_PrintUnformatted(response);
        ESP_LOGE(TAG, "Template Light flow did not create an entry: %s",
                 detail ? detail : "unknown response");
        cJSON_free(detail);
    }
    if (created) {
        const nvs_storage_write_item_t items[] = {
            {.key = "ha_url", .type = NVS_STORAGE_TYPE_STRING,
             .value.string = info->hass_url},
            {.key = "entry_id", .type = NVS_STORAGE_TYPE_STRING,
             .value.string = created_id},
            {.key = "action_schema", .type = NVS_STORAGE_TYPE_STRING,
             .value.string = "2"},
            {.key = "entity_seen", .type = NVS_STORAGE_TYPE_STRING,
             .value.string = "0"},
        };
        err = nvs_storage_write(STORAGE_NAMESPACE, items, 4);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not save created Template config entry ID");
            cJSON_Delete(response);
            return err;
        }
        cJSON_Delete(response);
        ESP_LOGI(TAG, "Created Template Light %s", state->light_id);
        for (int attempt = 0; attempt < 10 && !state->stop; attempt++) {
            bool registered = false;
            if (check_light_registry(state, &registered) == ESP_OK && registered) {
                esp_err_t save_err = mark_light_seen();
                if (save_err != ESP_OK) {
                    ESP_LOGW(TAG, "Light appeared but could not save marker: %s",
                             esp_err_to_name(save_err));
                }
                return ESP_OK;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        ESP_LOGE(TAG, "Template Light was created but %s did not appear",
                 state->light_id);
    } else {
        cJSON_Delete(response);
    }
    return ESP_FAIL;
}

static void command_event_cb(ha_ws_client_t *client, int subscription_id,
                             const cJSON *event, void *user_ctx)
{
    (void)client;
    (void)subscription_id;
    screen_light_t *state = user_ctx;
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(event, "data");
    if (!json_is(data, "device_id", state->mac)) {
        return;
    }
    ESP_LOGI(TAG, "HA light event: action=%s", json_string(data, "action") ?
             json_string(data, "action") : "missing");
    message_t msg = {.kind = MESSAGE_COMMAND};
    if (json_is(data, "action", "on")) {
        msg.command = COMMAND_ON;
    } else if (json_is(data, "action", "off")) {
        msg.command = COMMAND_OFF;
    } else if (json_is(data, "action", "level")) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(data, "brightness");
        bool raw_brightness = value != NULL &&
                              !(cJSON_IsString(value) &&
                                strspn(value->valuestring, " \t\r\n") ==
                                strlen(value->valuestring));
        if (!raw_brightness) {
            value = cJSON_GetObjectItemCaseSensitive(data, "brightness_pct");
        }
        const char *field = raw_brightness ? "brightness" : "brightness_pct";
        double number;
        if (cJSON_IsNumber(value)) {
            number = value->valuedouble;
        } else if (cJSON_IsString(value)) {
            char *end = NULL;
            number = strtod(value->valuestring, &end);
            while (end && isspace((unsigned char)*end)) {
                end++;
            }
            if (end == value->valuestring || !end || *end != '\0') {
                ESP_LOGW(TAG, "Invalid HA %s string: %.64s", field, value->valuestring);
                return;
            }
        } else {
            ESP_LOGW(TAG, "HA level event has no brightness or brightness_pct");
            return;
        }
        if (!isfinite(number) || number < 0 ||
            number > (raw_brightness ? 255 : 100)) {
            ESP_LOGW(TAG, "HA %s out of range: %f", field, number);
            return;
        }
        msg.command = COMMAND_LEVEL;
        msg.brightness = raw_brightness ? (int)(number * 100 / 255 + 0.5) :
                                          (int)(number + 0.5);
        ESP_LOGI(TAG, "HA requested screen brightness: %d%%", msg.brightness);
    } else {
        ESP_LOGW(TAG, "Unknown HA light action");
        return;
    }
    /* A slider can emit commands faster than the worker can persist them.
     * Keep the newest desired state instead of dropping the final value. */
    xQueueOverwrite(state->command_queue, &msg);
}

static esp_err_t report_state(screen_light_t *state, const char *domain,
                              const char *service, const char *entity_id,
                              bool include_value, int value)
{
    cJSON *target = cJSON_CreateObject();
    cJSON *data = include_value ? cJSON_CreateObject() : NULL;
    if (!target || (include_value && !data)) {
        cJSON_Delete(target);
        cJSON_Delete(data);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(target, "entity_id", entity_id);
    if (include_value) {
        cJSON_AddNumberToObject(data, "value", value);
    }
    esp_err_t err = ha_ws_call_service(state->client, domain, service, data, target,
                                        false, NULL, NULL, NULL);
    if (err == ESP_OK) {
        if (include_value) {
            ESP_LOGI(TAG, "HA state report queued: %s.%s %s value=%d", domain,
                     service, entity_id, value);
        } else {
            ESP_LOGI(TAG, "HA state report queued: %s.%s %s", domain, service,
                     entity_id);
        }
    } else {
        ESP_LOGW(TAG, "HA state report failed: %s.%s %s: %s", domain, service,
                 entity_id, esp_err_to_name(err));
    }
    cJSON_Delete(data);
    cJSON_Delete(target);
    return err;
}

static void sync_state(screen_light_t *state)
{
    if (!state->ready || !ha_ws_is_authenticated(state->client)) {
        return;
    }
    int brightness = bsp_lcd_br_get();
    int locked = screen_is_locked() ? 1 : 0;
    if (brightness != state->last_brightness) {
        if (report_state(state, "input_number", "set_value", state->brightness_id,
                         true, brightness) == ESP_OK) {
            state->last_brightness = brightness;
        }
    }
    if (locked != state->last_locked) {
        if (report_state(state, "input_boolean", locked ? "turn_on" : "turn_off",
                         state->lock_id, false, 0) == ESP_OK) {
            state->last_locked = locked;
        }
    }
}

static void apply_command(screen_light_t *state, const message_t *msg)
{
    if (msg->command == COMMAND_LEVEL) {
        ESP_LOGI(TAG, "Applying HA light command: level=%d%%", msg->brightness);
    } else {
        ESP_LOGI(TAG, "Applying HA light command: %s",
                 msg->command == COMMAND_OFF ? "off" : "on");
    }
    esp_err_t err = ESP_OK;
    if (msg->command == COMMAND_OFF) {
        err = screen_set_ha_lock(true);
    } else if (msg->command == COMMAND_ON) {
        device_info_t info;
        if (webserver_config_load_device_info_from_file(&info) == ESP_OK &&
            info.brightness == 0) {
            info.brightness = 50;
            if (webserver_config_save_device_info_to_file(&info) != ESP_OK) {
                ESP_LOGE(TAG, "Could not save default brightness before unlocking");
                return;
            }
            webserver_device_info_notify(UPDATE_BRIGHTNESS);
        }
        err = screen_set_ha_lock(false);
    } else {
        device_info_t info;
        if (webserver_config_load_device_info_from_file(&info) != ESP_OK) {
            ESP_LOGE(TAG, "Could not load device info for brightness command");
            return;
        }
        info.brightness = msg->brightness;
        if (webserver_config_save_device_info_to_file(&info) != ESP_OK) {
            ESP_LOGE(TAG, "Could not save requested brightness %d%%", msg->brightness);
            return;
        }
        night_mode_notify_manual_brightness();
        err = screen_set_ha_brightness(msg->brightness);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Screen brightness applied: requested=%d%% actual=%d%%",
                     msg->brightness, bsp_lcd_br_get());
            webserver_device_info_notify(UPDATE_BRIGHTNESS);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply HA light command: %s", esp_err_to_name(err));
    }
    sync_state(state);
}

static void worker(void *arg)
{
    screen_light_t *state = arg;
    bool provisioning_denied = false;
    ESP_LOGI(TAG, "Screen Light worker started");
    ha_auth_connection_info_t *info = calloc(1, sizeof(*info));
    if (!info || ha_auth_get_connection_info(info) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot load HA connection information");
        goto done;
    }
    while (!state->stop) {
        if (provisioning_denied) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (!ha_ws_is_authenticated(state->client)) {
            state->last_brightness = -1;
            state->last_locked = -1;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (!state->ready) {
            ESP_LOGI(TAG, "Checking HA Screen Light entities");
            const char *ids[] = {state->lock_id, state->brightness_id,
                                 state->light_id};
            cJSON *entries = NULL;
            esp_err_t err = get_registry_entries(state, ids, 3, &entries);
            if (err == ESP_OK) {
                err = ensure_helper(state, "input_boolean", state->lock_name,
                                    state->lock_id,
                                    cJSON_GetObjectItemCaseSensitive(entries,
                                                                     state->lock_id));
                if (err == ESP_OK) {
                    err = ensure_helper(state, "input_number",
                                        state->brightness_name, state->brightness_id,
                                        cJSON_GetObjectItemCaseSensitive(entries,
                                                                         state->brightness_id));
                }
                if (err == ESP_OK) {
                    err = ensure_light(state, info,
                                       cJSON_GetObjectItemCaseSensitive(entries,
                                                                        state->light_id));
                }
            }
            cJSON_Delete(entries);
            if (err == ESP_ERR_NOT_ALLOWED) {
                ESP_LOGE(TAG, "HA Screen Light setup requires administrator permission; "
                              "stopping setup attempts until the token changes or device restarts");
                provisioning_denied = true;
                continue;
            }
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Provisioning paused; retrying in 30 seconds");
                for (int i = 0; i < 60 && !state->stop; i++) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                continue;
            }
            state->ready = true;
            if (ha_ws_subscribe_events(state->client, EVENT_TYPE,
                                       command_event_cb, NULL, state, NULL) != ESP_OK) {
                state->ready = false;
                ESP_LOGW(TAG, "Could not subscribe to screen light commands");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            state->last_brightness = -1;
            state->last_locked = -1;
            ESP_LOGI(TAG, "Screen Light ready; subscribed to %s", EVENT_TYPE);
        }
        message_t msg;
        if (xQueueReceive(state->command_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            apply_command(state, &msg);
        }
        sync_state(state);
    }
done:
    ESP_LOGI(TAG, "Screen Light worker stopping");
    if (info) {
        memset(info->long_lived_token, 0, sizeof(info->long_lived_token));
        free(info);
    }
    xSemaphoreGive(state->done);
    vTaskDelete(NULL);
}

void ha_screen_light_start(ha_ws_client_t *client)
{
    screen_light_t *state = &s_light;
    if (!client || state->task) {
        return;
    }
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGE(TAG, "Could not read Wi-Fi MAC");
        return;
    }
    snprintf(state->mac, sizeof(state->mac), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    static const char hex_upper[] = "0123456789ABCDEF";
    static const char hex_lower[] = "0123456789abcdef";
    for (size_t i = 0; i < 2; i++) {
        state->suffix[2 * i] = hex_upper[mac[4 + i] >> 4];
        state->suffix[2 * i + 1] = hex_upper[mac[4 + i] & 0x0F];
        state->suffix_lower[2 * i] = hex_lower[mac[4 + i] >> 4];
        state->suffix_lower[2 * i + 1] = hex_lower[mac[4 + i] & 0x0F];
    }
    state->suffix[4] = '\0';
    state->suffix_lower[4] = '\0';
    snprintf(state->lock_id, sizeof(state->lock_id),
             "input_boolean.nextion_%.4s_screen_lock", state->suffix_lower);
    snprintf(state->brightness_id, sizeof(state->brightness_id),
             "input_number.nextion_%.4s_brightness", state->suffix_lower);
    snprintf(state->light_id, sizeof(state->light_id),
             "light.nextion_%.4s_screen", state->suffix_lower);
    snprintf(state->lock_name, sizeof(state->lock_name),
             "Nextion_%.4s Screen Lock", state->suffix);
    snprintf(state->brightness_name, sizeof(state->brightness_name),
             "Nextion_%.4s Brightness", state->suffix);
    snprintf(state->light_name, sizeof(state->light_name),
             "Nextion_%.4s Screen", state->suffix);
    if (!state->queue) {
        state->queue = xQueueCreate(12, sizeof(message_t));
    }
    if (!state->command_queue) {
        state->command_queue = xQueueCreate(1, sizeof(message_t));
    }
    if (!state->done) {
        state->done = xSemaphoreCreateBinary();
    }
    if (!state->queue || !state->command_queue || !state->done) {
        ESP_LOGE(TAG, "Could not allocate worker synchronization objects");
        return;
    }
    state->client = client;
    state->stop = false;
    state->ready = false;
    state->last_brightness = -1;
    state->last_locked = -1;
    xQueueReset(state->command_queue);
    ESP_LOGI(TAG, "Starting HA Screen Light: %s, %s, %s", state->lock_id,
             state->brightness_id, state->light_id);
    if (xTaskCreate(worker, "ha_screen_light", WORKER_STACK_SIZE, state, 4,
                    &state->task) != pdPASS) {
        state->task = NULL;
        ESP_LOGE(TAG, "Could not start worker");
    }
}

bool ha_screen_light_stop(void)
{
    screen_light_t *state = &s_light;
    if (!state->task) {
        return true;
    }
    state->stop = true;
    ESP_LOGI(TAG, "Stopping HA Screen Light");
    if (xSemaphoreTake(state->done, pdMS_TO_TICKS(20000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timed out stopping HA screen light worker");
        return false;
    }
    ha_ws_unsubscribe_events_by_callback(state->client, command_event_cb, state);
    ha_ws_cancel_pending_by_context(state->client, state);
    message_t msg;
    while (xQueueReceive(state->queue, &msg, 0) == pdTRUE) {
        cJSON_Delete(msg.result);
    }
    xQueueReset(state->command_queue);
    state->task = NULL;
    state->client = NULL;
    state->ready = false;
    ESP_LOGI(TAG, "HA Screen Light stopped");
    return true;
}
