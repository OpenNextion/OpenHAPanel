#include "ha_ws.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#define HA_WS_PINGPONG_TIMEOUT_SEC 60

typedef struct {
    bool active;
    bool is_subscription;
    int id;
    ha_ws_result_cb_t cb;
    void *user_ctx;
} ha_ws_pending_cmd_t;

typedef struct {
    bool active;
    int id;
    char *event_type;
    char **entity_ids;
    size_t entity_id_count;
    ha_ws_event_cb_t event_cb;
    ha_ws_result_cb_t result_cb;
    void *user_ctx;
} ha_ws_subscription_t;

struct ha_ws_client {
    esp_websocket_client_handle_t ws;
    char *hass_url;
    char *ws_url;
    char *access_token;
    char ha_version[32];
    int next_id;
    bool ws_open;
    bool authenticated;
    bool closing;
    bool auto_reconnect;
    bool coalesce_messages;
    bool connection_slot_pending;
    bool owns_connection_slot;
    SemaphoreHandle_t lock;
    ha_ws_connection_cb_t connection_cb;
    void *user_ctx;
    char *rx_buf;
    size_t rx_capacity;
    size_t rx_len;
    ha_ws_pending_cmd_t pending[CONFIG_HA_WS_MAX_PENDING];
    ha_ws_subscription_t subscriptions[CONFIG_HA_WS_MAX_SUBSCRIPTIONS];
};

static const char *TAG = "ha_ws";
static SemaphoreHandle_t s_connection_slot;
static portMUX_TYPE s_connection_slot_guard = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t ha_ws_send_json_locked(ha_ws_client_t *client, cJSON *message);
static esp_err_t ha_ws_resubscribe_locked(ha_ws_client_t *client);

static SemaphoreHandle_t ha_ws_get_connection_slot(void)
{
    taskENTER_CRITICAL(&s_connection_slot_guard);
    SemaphoreHandle_t slot = s_connection_slot;
    taskEXIT_CRITICAL(&s_connection_slot_guard);
    if (slot) {
        return slot;
    }

    SemaphoreHandle_t created = xSemaphoreCreateBinary();
    if (!created) {
        return NULL;
    }
    xSemaphoreGive(created);

    taskENTER_CRITICAL(&s_connection_slot_guard);
    if (!s_connection_slot) {
        s_connection_slot = created;
        created = NULL;
    }
    slot = s_connection_slot;
    taskEXIT_CRITICAL(&s_connection_slot_guard);
    if (created) {
        vSemaphoreDelete(created);
    }
    return slot;
}

static void ha_ws_release_connection_slot(ha_ws_client_t *client)
{
    bool release = false;
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    if (client->owns_connection_slot) {
        client->owns_connection_slot = false;
        release = true;
    }
    client->connection_slot_pending = false;
    xSemaphoreGiveRecursive(client->lock);
    if (release) {
        xSemaphoreGive(s_connection_slot);
    }
}

static size_t ha_ws_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

static char *ha_ws_strdup(const char *value)
{
    if (!value) {
        return NULL;
    }
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, value, len);
    }
    return copy;
}

static void ha_ws_secure_free(char *value)
{
    if (value) {
        volatile char *p = value;
        size_t length = strlen(value);
        while (length--) {
            *p++ = '\0';
        }
        free(value);
    }
}

static char *ha_ws_build_ws_url(const char *hass_url)
{
    const char *prefix = NULL;
    size_t prefix_len = 0;
    const char *rest = NULL;

    if (strncmp(hass_url, "http://", 7) == 0) {
        prefix = "ws://";
        prefix_len = 5;
        rest = hass_url + 7;
    } else if (strncmp(hass_url, "https://", 8) == 0) {
        prefix = "wss://";
        prefix_len = 6;
        rest = hass_url + 8;
    } else if (strncmp(hass_url, "ws://", 5) == 0 || strncmp(hass_url, "wss://", 6) == 0) {
        const char *path = strstr(hass_url, "/api/websocket");
        if (path) {
            return ha_ws_strdup(hass_url);
        }
        prefix = "";
        prefix_len = 0;
        rest = hass_url;
    } else {
        prefix = "ws://";
        prefix_len = 5;
        rest = hass_url;
    }

    size_t rest_len = strlen(rest);
    while (rest_len > 0 && rest[rest_len - 1] == '/') {
        rest_len--;
    }

    const char *suffix = "/api/websocket";
    size_t suffix_len = strlen(suffix);
    char *url = malloc(prefix_len + rest_len + suffix_len + 1);
    if (!url) {
        return NULL;
    }

    memcpy(url, prefix, prefix_len);
    memcpy(url + prefix_len, rest, rest_len);
    memcpy(url + prefix_len + rest_len, suffix, suffix_len + 1);
    return url;
}

static void ha_ws_fire_connection_event(ha_ws_client_t *client, ha_ws_event_t event)
{
    if (client->connection_cb) {
        client->connection_cb(client, event, client->user_ctx);
    }
}

static ha_ws_pending_cmd_t *ha_ws_find_pending(ha_ws_client_t *client, int id)
{
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_PENDING; i++) {
        if (client->pending[i].active && client->pending[i].id == id) {
            return &client->pending[i];
        }
    }
    return NULL;
}

static ha_ws_pending_cmd_t *ha_ws_alloc_pending(ha_ws_client_t *client)
{
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_PENDING; i++) {
        if (!client->pending[i].active) {
            return &client->pending[i];
        }
    }
    return NULL;
}

static ha_ws_subscription_t *ha_ws_find_subscription(ha_ws_client_t *client, int id)
{
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_SUBSCRIPTIONS; i++) {
        if (client->subscriptions[i].active && client->subscriptions[i].id == id) {
            return &client->subscriptions[i];
        }
    }
    return NULL;
}

static ha_ws_subscription_t *ha_ws_alloc_subscription(ha_ws_client_t *client)
{
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_SUBSCRIPTIONS; i++) {
        if (!client->subscriptions[i].active) {
            return &client->subscriptions[i];
        }
    }
    return NULL;
}

static void ha_ws_free_subscription(ha_ws_subscription_t *subscription)
{
    if (!subscription) {
        return;
    }
    free(subscription->event_type);
    for (size_t i = 0; i < subscription->entity_id_count; i++) {
        free(subscription->entity_ids[i]);
    }
    free(subscription->entity_ids);
    memset(subscription, 0, sizeof(*subscription));
}

static esp_err_t ha_ws_copy_entity_ids(ha_ws_subscription_t *subscription,
                                       const char *const *entity_ids,
                                       size_t entity_id_count)
{
    subscription->entity_ids = calloc(entity_id_count, sizeof(*subscription->entity_ids));
    if (!subscription->entity_ids) {
        return ESP_ERR_NO_MEM;
    }
    subscription->entity_id_count = entity_id_count;
    for (size_t i = 0; i < entity_id_count; i++) {
        subscription->entity_ids[i] = ha_ws_strdup(entity_ids[i]);
        if (!subscription->entity_ids[i]) {
            ha_ws_free_subscription(subscription);
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

static bool ha_ws_add_entity_ids(cJSON *message,
                                 const char *const *entity_ids,
                                 size_t entity_id_count)
{
    cJSON *array = cJSON_AddArrayToObject(message, "entity_ids");
    if (!array) {
        return false;
    }
    for (size_t i = 0; i < entity_id_count; i++) {
        cJSON *item = cJSON_CreateString(entity_ids[i]);
        if (!item || !cJSON_AddItemToArray(array, item)) {
            cJSON_Delete(item);
            return false;
        }
    }
    return true;
}

static void ha_ws_reject_pending_locked(ha_ws_client_t *client, esp_err_t err)
{
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_PENDING; i++) {
        ha_ws_pending_cmd_t *pending = &client->pending[i];
        if (!pending->active) {
            continue;
        }
        if (pending->cb) {
            pending->cb(client, pending->id, err, NULL, NULL, pending->user_ctx);
        }
        memset(pending, 0, sizeof(*pending));
    }
}

static esp_err_t ha_ws_ensure_rx_capacity_locked(ha_ws_client_t *client, size_t required)
{
    if (required <= client->rx_capacity) {
        return ESP_OK;
    }

    char *new_buf = realloc(client->rx_buf, required);
    if (!new_buf) {
        ESP_LOGE(TAG, "Failed to grow RX buffer to %u bytes", (unsigned)required);
        client->rx_len = 0;
        return ESP_ERR_NO_MEM;
    }

    client->rx_buf = new_buf;
    client->rx_capacity = required;
    return ESP_OK;
}

static esp_err_t ha_ws_send_auth_locked(ha_ws_client_t *client)
{
    cJSON *message = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to create auth message");
    cJSON_AddStringToObject(message, "type", "auth");
    cJSON_AddStringToObject(message, "access_token", client->access_token);
    esp_err_t err = ha_ws_send_json_locked(client, message);
    cJSON_Delete(message);
    return err;
}

static esp_err_t ha_ws_send_supported_features_locked(ha_ws_client_t *client)
{
    cJSON *message = cJSON_CreateObject();
    cJSON *features = cJSON_CreateObject();
    if (!message || !features) {
        cJSON_Delete(message);
        cJSON_Delete(features);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(message, "id", 1);
    cJSON_AddStringToObject(message, "type", "supported_features");
    cJSON_AddItemToObject(message, "features", features);
    cJSON_AddNumberToObject(features, "coalesce_messages", 1);

    esp_err_t err = ha_ws_send_json_locked(client, message);
    cJSON_Delete(message);
    return err;
}

static esp_err_t ha_ws_send_json_locked(ha_ws_client_t *client, cJSON *message)
{
    char *payload = cJSON_PrintUnformatted(message);
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "Failed to render JSON");

    int written = esp_websocket_client_send_text(client->ws,
                                                payload,
                                                strlen(payload),
                                                pdMS_TO_TICKS(CONFIG_HA_WS_TX_TIMEOUT_MS));
    free(payload);
    return written < 0 ? ESP_FAIL : ESP_OK;
}

static void ha_ws_handle_auth_ok_locked(ha_ws_client_t *client, const cJSON *message)
{
    const cJSON *ha_version = cJSON_GetObjectItemCaseSensitive(message, "ha_version");
    if (cJSON_IsString(ha_version) && ha_version->valuestring) {
        strlcpy(client->ha_version, ha_version->valuestring, sizeof(client->ha_version));
    } else {
        client->ha_version[0] = '\0';
    }
    client->authenticated = true;
    ESP_LOGI(TAG, "Authenticated with Home Assistant %s", client->ha_version);
    if (client->coalesce_messages) {
        esp_err_t err = ha_ws_send_supported_features_locked(client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to enable coalesced messages: %s", esp_err_to_name(err));
        }
    }
    ha_ws_resubscribe_locked(client);
    ha_ws_fire_connection_event(client, HA_WS_EVENT_AUTHENTICATED);
}

static void ha_ws_handle_result_locked(ha_ws_client_t *client, const cJSON *message)
{
    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(message, "id");
    const cJSON *success_item = cJSON_GetObjectItemCaseSensitive(message, "success");
    if (!cJSON_IsNumber(id_item) || !cJSON_IsBool(success_item)) {
        ESP_LOGW(TAG, "Invalid result message");
        return;
    }

    int id = id_item->valueint;
    ha_ws_pending_cmd_t *pending = ha_ws_find_pending(client, id);
    if (!pending) {
        if (id == 1) {
            return;
        }
        ESP_LOGW(TAG, "Result for unknown command id %d", id);
        return;
    }

    bool success = cJSON_IsTrue(success_item);
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(message, "result");
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(message, "error");

    if (pending->cb) {
        pending->cb(client, id, success ? ESP_OK : ESP_FAIL, result, error, pending->user_ctx);
    }

    if (!success || !pending->is_subscription) {
        memset(pending, 0, sizeof(*pending));
    }
}

static void ha_ws_handle_pong_locked(ha_ws_client_t *client, const cJSON *message)
{
    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(message, "id");
    if (!cJSON_IsNumber(id_item)) {
        ESP_LOGW(TAG, "Invalid pong message");
        return;
    }

    int id = id_item->valueint;
    ha_ws_pending_cmd_t *pending = ha_ws_find_pending(client, id);
    if (!pending) {
        ESP_LOGW(TAG, "Pong for unknown command id %d", id);
        return;
    }

    if (pending->cb) {
        pending->cb(client, id, ESP_OK, NULL, NULL, pending->user_ctx);
    }
    memset(pending, 0, sizeof(*pending));
}

static void ha_ws_handle_event_locked(ha_ws_client_t *client, const cJSON *message)
{
    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(message, "id");
    const cJSON *event = cJSON_GetObjectItemCaseSensitive(message, "event");
    if (!cJSON_IsNumber(id_item) || !event) {
        ESP_LOGW(TAG, "Invalid event message");
        return;
    }

    int id = id_item->valueint;
    ha_ws_subscription_t *subscription = ha_ws_find_subscription(client, id);
    if (!subscription) {
        ESP_LOGW(TAG, "Event for unknown subscription id %d", id);
        return;
    }

    if (subscription->event_cb) {
        subscription->event_cb(client, id, event, subscription->user_ctx);
    }
}

static void ha_ws_handle_one_message_locked(ha_ws_client_t *client, const cJSON *message)
{
    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(message, "type");
    if (!cJSON_IsString(type_item) || !type_item->valuestring) {
        ESP_LOGW(TAG, "Message without type");
        return;
    }

    const char *type = type_item->valuestring;
    if (strcmp(type, "auth_required") == 0) {
        ha_ws_fire_connection_event(client, HA_WS_EVENT_AUTH_REQUIRED);
        ha_ws_send_auth_locked(client);
    } else if (strcmp(type, "auth_ok") == 0) {
        ha_ws_handle_auth_ok_locked(client, message);
    } else if (strcmp(type, "auth_invalid") == 0) {
        client->authenticated = false;
        ha_ws_fire_connection_event(client, HA_WS_EVENT_AUTH_INVALID);
    } else if (strcmp(type, "result") == 0) {
        ha_ws_handle_result_locked(client, message);
    } else if (strcmp(type, "pong") == 0) {
        ha_ws_handle_pong_locked(client, message);
    } else if (strcmp(type, "event") == 0) {
        ha_ws_handle_event_locked(client, message);
    } else {
        ESP_LOGD(TAG, "Unhandled message type %s", type);
    }
}

static void ha_ws_handle_payload(ha_ws_client_t *client, const char *data, int len)
{
    cJSON *message = cJSON_ParseWithLength(data, len);
    if (!message) {
        ESP_LOGW(TAG, "Failed to parse WebSocket payload");
        return;
    }

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    if (cJSON_IsArray(message)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, message) {
            if (cJSON_IsObject(item)) {
                ha_ws_handle_one_message_locked(client, item);
            }
        }
    } else if (cJSON_IsObject(message)) {
        ha_ws_handle_one_message_locked(client, message);
    }
    xSemaphoreGiveRecursive(client->lock);

    cJSON_Delete(message);
}

static void ha_ws_event_handler(void *handler_args,
                                esp_event_base_t base,
                                int32_t event_id,
                                void *event_data)
{
    (void)base;
    ha_ws_client_t *client = handler_args;
    esp_websocket_event_data_t *data = event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
        client->ws_open = true;
        client->authenticated = false;
        client->rx_len = 0;
        xSemaphoreGiveRecursive(client->lock);
        ha_ws_fire_connection_event(client, HA_WS_EVENT_CONNECTED);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
        client->ws_open = false;
        client->authenticated = false;
        client->rx_len = 0;
        ha_ws_reject_pending_locked(client, ESP_ERR_INVALID_STATE);
        xSemaphoreGiveRecursive(client->lock);
        ha_ws_fire_connection_event(client, HA_WS_EVENT_DISCONNECTED);
        break;

    case WEBSOCKET_EVENT_DATA:
        if (!data || data->data_len <= 0 || data->payload_len <= 0) {
            break;
        }
        if ((size_t)data->payload_len > CONFIG_HA_WS_RX_BUFFER_SIZE ||
            data->payload_offset < 0 ||
            data->data_len < 0 ||
            data->payload_offset + data->data_len > data->payload_len) {
            ESP_LOGW(TAG,
                     "Dropping oversized or invalid payload: offset=%d data_len=%d payload_len=%d max=%d",
                     data->payload_offset,
                     data->data_len,
                     data->payload_len,
                     CONFIG_HA_WS_RX_BUFFER_SIZE);
            xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
            client->rx_len = 0;
            xSemaphoreGiveRecursive(client->lock);
            break;
        }

        xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
        esp_err_t rx_err = ha_ws_ensure_rx_capacity_locked(client, (size_t)data->payload_len + 1);
        bool complete = false;
        if (rx_err == ESP_OK) {
            memcpy(client->rx_buf + data->payload_offset, data->data_ptr, data->data_len);
            client->rx_len = data->payload_offset + data->data_len;
            complete = client->rx_len == (size_t)data->payload_len;
            if (complete) {
                client->rx_buf[data->payload_len] = '\0';
            }
        }
        xSemaphoreGiveRecursive(client->lock);

        if (complete) {
            ESP_LOGI(TAG, "Received HA JSON payload: %d bytes", data->payload_len);
            ha_ws_handle_payload(client, client->rx_buf, data->payload_len);
            xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
            client->rx_len = 0;
            xSemaphoreGiveRecursive(client->lock);
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ha_ws_fire_connection_event(client, HA_WS_EVENT_ERROR);
        break;

    default:
        break;
    }
}

esp_err_t ha_ws_client_init(const ha_ws_config_t *config, ha_ws_client_t **out_client)
{
    ESP_RETURN_ON_FALSE(config && out_client, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(config->hass_url && config->access_token, ESP_ERR_INVALID_ARG, TAG, "Missing HA URL or token");

    ESP_RETURN_ON_FALSE(ha_ws_get_connection_slot(), ESP_ERR_NO_MEM, TAG, "Failed to create connection slot");

    ha_ws_client_t *client = calloc(1, sizeof(*client));
    ESP_RETURN_ON_FALSE(client, ESP_ERR_NO_MEM, TAG, "Failed to allocate client");

    client->hass_url = ha_ws_strdup(config->hass_url);
    client->ws_url = ha_ws_build_ws_url(config->hass_url);
    client->access_token = ha_ws_strdup(config->access_token);
    client->lock = xSemaphoreCreateRecursiveMutex();
    client->rx_capacity = ha_ws_min_size(4096, CONFIG_HA_WS_RX_BUFFER_SIZE + 1);
    client->rx_buf = heap_caps_malloc(client->rx_capacity,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!client->rx_buf) {
        client->rx_buf = malloc(client->rx_capacity);
        if (client->rx_buf) {
            ESP_LOGW(TAG, "PSRAM receive buffer allocation failed; using internal RAM");
        }
    }
    client->next_id = 1;
    client->auto_reconnect = config->auto_reconnect;
    client->coalesce_messages = config->coalesce_messages;
    client->connection_cb = config->connection_cb;
    client->user_ctx = config->user_ctx;

    if (!client->hass_url || !client->ws_url || !client->access_token || !client->lock || !client->rx_buf) {
        ha_ws_destroy(client);
        return ESP_ERR_NO_MEM;
    }

    esp_websocket_client_config_t ws_config = {
        .uri = client->ws_url,
        .disable_auto_reconnect = !client->auto_reconnect,
        .enable_close_reconnect = client->auto_reconnect,
        .reconnect_timeout_ms = client->auto_reconnect ? 5000 : 0,
        .network_timeout_ms = 10000,
        .pingpong_timeout_sec = HA_WS_PINGPONG_TIMEOUT_SEC,
        .task_stack = CONFIG_HA_WS_TASK_STACK_SIZE,
        .buffer_size = ha_ws_min_size(4096, CONFIG_HA_WS_RX_BUFFER_SIZE),
        .user_context = client,
    };

    client->ws = esp_websocket_client_init(&ws_config);
    if (!client->ws) {
        ha_ws_destroy(client);
        return ESP_FAIL;
    }

    esp_err_t err = esp_websocket_register_events(client->ws,
                                                   WEBSOCKET_EVENT_ANY,
                                                   ha_ws_event_handler,
                                                   client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket events: %s", esp_err_to_name(err));
        ha_ws_destroy(client);
        return err;
    }

    *out_client = client;
    return ESP_OK;
}

static esp_err_t ha_ws_connect_with_ticks(ha_ws_client_t *client, TickType_t wait_ticks)
{
    ESP_RETURN_ON_FALSE(client && client->ws, ESP_ERR_INVALID_ARG, TAG, "Invalid client");
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    if (client->owns_connection_slot || client->connection_slot_pending) {
        xSemaphoreGiveRecursive(client->lock);
        return ESP_ERR_INVALID_STATE;
    }
    client->connection_slot_pending = true;
    client->closing = false;
    xSemaphoreGiveRecursive(client->lock);

    if (xSemaphoreTake(s_connection_slot, wait_ticks) != pdTRUE) {
        xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
        client->connection_slot_pending = false;
        xSemaphoreGiveRecursive(client->lock);
        return ESP_ERR_TIMEOUT;
    }

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    client->connection_slot_pending = false;
    client->owns_connection_slot = true;
    xSemaphoreGiveRecursive(client->lock);

    esp_err_t err = esp_websocket_client_start(client->ws);
    if (err != ESP_OK) {
        ha_ws_release_connection_slot(client);
    }
    return err;
}

esp_err_t ha_ws_connect(ha_ws_client_t *client)
{
    return ha_ws_connect_with_ticks(client, portMAX_DELAY);
}

esp_err_t ha_ws_connect_timeout(ha_ws_client_t *client, uint32_t timeout_ms)
{
    return ha_ws_connect_with_ticks(client, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t ha_ws_close(ha_ws_client_t *client)
{
    ESP_RETURN_ON_FALSE(client && client->ws, ESP_ERR_INVALID_ARG, TAG, "Invalid client");
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    client->closing = true;
    xSemaphoreGiveRecursive(client->lock);
    esp_err_t err = esp_websocket_client_stop(client->ws);
    if (err == ESP_OK) {
        ha_ws_release_connection_slot(client);
    }
    return err;
}

esp_err_t ha_ws_destroy(ha_ws_client_t *client)
{
    if (!client) {
        return ESP_OK;
    }

    if (client->ws) {
        esp_err_t err = esp_websocket_client_destroy(client->ws);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to destroy WebSocket client: %s", esp_err_to_name(err));
            return err;
        }
        client->ws = NULL;
    }
    if (client->lock) {
        ha_ws_release_connection_slot(client);
    }
    if (client->lock) {
        vSemaphoreDelete(client->lock);
    }
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_SUBSCRIPTIONS; i++) {
        ha_ws_free_subscription(&client->subscriptions[i]);
    }
    free(client->hass_url);
    free(client->ws_url);
    ha_ws_secure_free(client->access_token);
    free(client->rx_buf);
    free(client);
    return ESP_OK;
}

bool ha_ws_is_authenticated(ha_ws_client_t *client)
{
    if (!client) {
        return false;
    }
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    bool authenticated = client->authenticated;
    xSemaphoreGiveRecursive(client->lock);
    return authenticated;
}

const char *ha_ws_get_ha_version(ha_ws_client_t *client)
{
    return client ? client->ha_version : "";
}

esp_err_t ha_ws_send_command(ha_ws_client_t *client,
                             cJSON *message,
                             ha_ws_result_cb_t cb,
                             void *user_ctx,
                             int *id_out)
{
    ESP_RETURN_ON_FALSE(client && message, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    if (!client->authenticated) {
        xSemaphoreGiveRecursive(client->lock);
        return HA_WS_ERR_NOT_AUTHENTICATED;
    }

    ha_ws_pending_cmd_t *pending = ha_ws_alloc_pending(client);
    if (!pending) {
        xSemaphoreGiveRecursive(client->lock);
        return HA_WS_ERR_NO_FREE_SLOT;
    }

    int id = ++client->next_id;
    cJSON_AddNumberToObject(message, "id", id);
    pending->active = true;
    pending->is_subscription = false;
    pending->id = id;
    pending->cb = cb;
    pending->user_ctx = user_ctx;

    esp_err_t err = ha_ws_send_json_locked(client, message);
    if (err != ESP_OK) {
        memset(pending, 0, sizeof(*pending));
    } else if (id_out) {
        *id_out = id;
    }
    xSemaphoreGiveRecursive(client->lock);
    return err;
}

static esp_err_t ha_ws_simple_command(ha_ws_client_t *client,
                                      const char *type,
                                      ha_ws_result_cb_t cb,
                                      void *user_ctx,
                                      int *id_out)
{
    cJSON *message = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to create command");
    cJSON_AddStringToObject(message, "type", type);
    esp_err_t err = ha_ws_send_command(client, message, cb, user_ctx, id_out);
    cJSON_Delete(message);
    return err;
}

esp_err_t ha_ws_get_states(ha_ws_client_t *client, ha_ws_result_cb_t cb, void *user_ctx, int *id_out)
{
    return ha_ws_simple_command(client, "get_states", cb, user_ctx, id_out);
}

esp_err_t ha_ws_get_config(ha_ws_client_t *client, ha_ws_result_cb_t cb, void *user_ctx, int *id_out)
{
    return ha_ws_simple_command(client, "get_config", cb, user_ctx, id_out);
}

esp_err_t ha_ws_get_services(ha_ws_client_t *client, ha_ws_result_cb_t cb, void *user_ctx, int *id_out)
{
    return ha_ws_simple_command(client, "get_services", cb, user_ctx, id_out);
}

esp_err_t ha_ws_get_user(ha_ws_client_t *client, ha_ws_result_cb_t cb, void *user_ctx, int *id_out)
{
    return ha_ws_simple_command(client, "auth/current_user", cb, user_ctx, id_out);
}

esp_err_t ha_ws_ping(ha_ws_client_t *client, ha_ws_result_cb_t cb, void *user_ctx, int *id_out)
{
    return ha_ws_simple_command(client, "ping", cb, user_ctx, id_out);
}

esp_err_t ha_ws_call_service(ha_ws_client_t *client,
                             const char *domain,
                             const char *service,
                             const cJSON *service_data,
                             const cJSON *target,
                             bool return_response,
                             ha_ws_result_cb_t cb,
                             void *user_ctx,
                             int *id_out)
{
    ESP_RETURN_ON_FALSE(domain && service, ESP_ERR_INVALID_ARG, TAG, "Missing service");
    cJSON *message = cJSON_CreateObject();
    if (!message) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(message, "type", "call_service");
    cJSON_AddStringToObject(message, "domain", domain);
    cJSON_AddStringToObject(message, "service", service);
    cJSON_AddBoolToObject(message, "return_response", return_response);
    if (service_data) {
        cJSON_AddItemToObject(message, "service_data", cJSON_Duplicate(service_data, true));
    }
    if (target) {
        cJSON_AddItemToObject(message, "target", cJSON_Duplicate(target, true));
    }

    esp_err_t err = ha_ws_send_command(client, message, cb, user_ctx, id_out);
    cJSON_Delete(message);
    return err;
}

esp_err_t ha_ws_subscribe_events(ha_ws_client_t *client,
                                 const char *event_type,
                                 ha_ws_event_cb_t event_cb,
                                 ha_ws_result_cb_t result_cb,
                                 void *user_ctx,
                                 int *subscription_id_out)
{
    ESP_RETURN_ON_FALSE(client && event_cb, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    cJSON *message = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to create subscribe command");
    cJSON_AddStringToObject(message, "type", "subscribe_events");
    if (event_type) {
        cJSON_AddStringToObject(message, "event_type", event_type);
    }

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    if (!client->authenticated) {
        xSemaphoreGiveRecursive(client->lock);
        cJSON_Delete(message);
        return HA_WS_ERR_NOT_AUTHENTICATED;
    }

    ha_ws_pending_cmd_t *pending = ha_ws_alloc_pending(client);
    ha_ws_subscription_t *subscription = ha_ws_alloc_subscription(client);
    if (!pending || !subscription) {
        xSemaphoreGiveRecursive(client->lock);
        cJSON_Delete(message);
        return HA_WS_ERR_NO_FREE_SLOT;
    }

    int id = ++client->next_id;
    cJSON_AddNumberToObject(message, "id", id);
    subscription->active = true;
    subscription->id = id;
    subscription->event_type = ha_ws_strdup(event_type);
    subscription->event_cb = event_cb;
    subscription->result_cb = result_cb;
    subscription->user_ctx = user_ctx;

    pending->active = true;
    pending->is_subscription = true;
    pending->id = id;
    pending->cb = result_cb;
    pending->user_ctx = user_ctx;

    esp_err_t err = ha_ws_send_json_locked(client, message);
    if (err != ESP_OK) {
        free(subscription->event_type);
        memset(subscription, 0, sizeof(*subscription));
        memset(pending, 0, sizeof(*pending));
    } else if (subscription_id_out) {
        *subscription_id_out = id;
    }
    xSemaphoreGiveRecursive(client->lock);

    cJSON_Delete(message);
    return err;
}

esp_err_t ha_ws_subscribe_entities(ha_ws_client_t *client,
                                   const char *const *entity_ids,
                                   size_t entity_id_count,
                                   ha_ws_event_cb_t event_cb,
                                   ha_ws_result_cb_t result_cb,
                                   void *user_ctx,
                                   int *subscription_id_out)
{
    ESP_RETURN_ON_FALSE(client && entity_ids && entity_id_count > 0 && event_cb,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Invalid argument");
    for (size_t i = 0; i < entity_id_count; i++) {
        ESP_RETURN_ON_FALSE(entity_ids[i] && entity_ids[i][0],
                            ESP_ERR_INVALID_ARG,
                            TAG,
                            "Invalid entity id");
    }

    cJSON *message = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to create subscribe command");
    cJSON_AddStringToObject(message, "type", "subscribe_entities");
    if (!ha_ws_add_entity_ids(message, entity_ids, entity_id_count)) {
        cJSON_Delete(message);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    if (!client->authenticated) {
        xSemaphoreGiveRecursive(client->lock);
        cJSON_Delete(message);
        return HA_WS_ERR_NOT_AUTHENTICATED;
    }

    ha_ws_pending_cmd_t *pending = ha_ws_alloc_pending(client);
    ha_ws_subscription_t *subscription = ha_ws_alloc_subscription(client);
    if (!pending || !subscription) {
        xSemaphoreGiveRecursive(client->lock);
        cJSON_Delete(message);
        return HA_WS_ERR_NO_FREE_SLOT;
    }

    esp_err_t err = ha_ws_copy_entity_ids(subscription, entity_ids, entity_id_count);
    if (err != ESP_OK) {
        xSemaphoreGiveRecursive(client->lock);
        cJSON_Delete(message);
        return err;
    }

    int id = ++client->next_id;
    cJSON_AddNumberToObject(message, "id", id);
    subscription->active = true;
    subscription->id = id;
    subscription->event_cb = event_cb;
    subscription->result_cb = result_cb;
    subscription->user_ctx = user_ctx;

    pending->active = true;
    pending->is_subscription = true;
    pending->id = id;
    pending->cb = result_cb;
    pending->user_ctx = user_ctx;

    err = ha_ws_send_json_locked(client, message);
    if (err != ESP_OK) {
        ha_ws_free_subscription(subscription);
        memset(pending, 0, sizeof(*pending));
    } else if (subscription_id_out) {
        *subscription_id_out = id;
    }
    xSemaphoreGiveRecursive(client->lock);

    cJSON_Delete(message);
    return err;
}

esp_err_t ha_ws_unsubscribe_events(ha_ws_client_t *client,
                                   int subscription_id,
                                   ha_ws_result_cb_t cb,
                                   void *user_ctx,
                                   int *id_out)
{
    ESP_RETURN_ON_FALSE(client, ESP_ERR_INVALID_ARG, TAG, "Invalid client");

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    ha_ws_subscription_t *subscription = ha_ws_find_subscription(client, subscription_id);
    if (!subscription) {
        xSemaphoreGiveRecursive(client->lock);
        return ESP_ERR_NOT_FOUND;
    }
    ha_ws_free_subscription(subscription);

    ha_ws_pending_cmd_t *pending_subscription = ha_ws_find_pending(client, subscription_id);
    if (pending_subscription) {
        memset(pending_subscription, 0, sizeof(*pending_subscription));
    }
    xSemaphoreGiveRecursive(client->lock);

    cJSON *message = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to create unsubscribe command");
    cJSON_AddStringToObject(message, "type", "unsubscribe_events");
    cJSON_AddNumberToObject(message, "subscription", subscription_id);
    esp_err_t err = ha_ws_send_command(client, message, cb, user_ctx, id_out);
    cJSON_Delete(message);
    return err;
}

esp_err_t ha_ws_unsubscribe_events_by_callback(ha_ws_client_t *client,
                                               ha_ws_event_cb_t event_cb,
                                               void *event_user_ctx)
{
    ESP_RETURN_ON_FALSE(client && event_cb, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    int subscription_ids[CONFIG_HA_WS_MAX_SUBSCRIPTIONS] = {0};
    size_t match_count = 0;
    bool authenticated = false;

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_SUBSCRIPTIONS; i++) {
        ha_ws_subscription_t *subscription = &client->subscriptions[i];
        if (subscription->active && subscription->event_cb == event_cb &&
            subscription->user_ctx == event_user_ctx) {
            int subscription_id = subscription->id;
            subscription_ids[match_count++] = subscription_id;
            ha_ws_free_subscription(subscription);

            ha_ws_pending_cmd_t *pending_subscription =
                ha_ws_find_pending(client, subscription_id);
            if (pending_subscription) {
                memset(pending_subscription, 0, sizeof(*pending_subscription));
            }
        }
    }

    if (match_count == 0) {
        xSemaphoreGiveRecursive(client->lock);
        return ESP_ERR_NOT_FOUND;
    }

    authenticated = client->authenticated;
    xSemaphoreGiveRecursive(client->lock);

    if (!authenticated) {
        return ESP_OK;
    }

    esp_err_t first_error = ESP_OK;
    for (size_t i = 0; i < match_count; i++) {
        cJSON *message = cJSON_CreateObject();
        if (!message) {
            if (first_error == ESP_OK) {
                first_error = ESP_ERR_NO_MEM;
            }
            continue;
        }
        cJSON_AddStringToObject(message, "type", "unsubscribe_events");
        cJSON_AddNumberToObject(message, "subscription", subscription_ids[i]);
        esp_err_t err = ha_ws_send_command(client, message, NULL, NULL, NULL);
        cJSON_Delete(message);
        if (first_error == ESP_OK && err != ESP_OK) {
            first_error = err;
        }
    }
    return first_error;
}

esp_err_t ha_ws_cancel_pending_by_context(ha_ws_client_t *client, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(client, ESP_ERR_INVALID_ARG, TAG, "Invalid client");

    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_PENDING; i++) {
        ha_ws_pending_cmd_t *pending = &client->pending[i];
        if (pending->active && pending->user_ctx == user_ctx) {
            /* Keep an active subscription command owned by its subscription. */
            if (pending->is_subscription && ha_ws_find_subscription(client, pending->id)) {
                pending->cb = NULL;
                pending->user_ctx = NULL;
            } else {
                memset(pending, 0, sizeof(*pending));
            }
        }
    }
    xSemaphoreGiveRecursive(client->lock);
    return ESP_OK;
}

static esp_err_t ha_ws_resubscribe_locked(ha_ws_client_t *client)
{
    for (size_t i = 0; i < CONFIG_HA_WS_MAX_SUBSCRIPTIONS; i++) {
        ha_ws_subscription_t *subscription = &client->subscriptions[i];
        if (!subscription->active) {
            continue;
        }

        cJSON *message = cJSON_CreateObject();
        if (!message) {
            return ESP_ERR_NO_MEM;
        }
        if (subscription->entity_ids) {
            cJSON_AddStringToObject(message, "type", "subscribe_entities");
            if (!ha_ws_add_entity_ids(message,
                                      (const char *const *)subscription->entity_ids,
                                      subscription->entity_id_count)) {
                cJSON_Delete(message);
                return ESP_ERR_NO_MEM;
            }
        } else {
            cJSON_AddStringToObject(message, "type", "subscribe_events");
            if (subscription->event_type) {
                cJSON_AddStringToObject(message, "event_type", subscription->event_type);
            }
        }

        int old_id = subscription->id;
        int new_id = ++client->next_id;
        cJSON_AddNumberToObject(message, "id", new_id);
        subscription->id = new_id;

        ha_ws_pending_cmd_t *pending = ha_ws_find_pending(client, old_id);
        if (!pending) {
            pending = ha_ws_alloc_pending(client);
        }
        if (pending) {
            pending->active = true;
            pending->is_subscription = true;
            pending->id = new_id;
            pending->cb = subscription->result_cb;
            pending->user_ctx = subscription->user_ctx;
        }

        esp_err_t err = ha_ws_send_json_locked(client, message);
        cJSON_Delete(message);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}
