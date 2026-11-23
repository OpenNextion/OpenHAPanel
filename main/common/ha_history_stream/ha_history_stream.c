#include "ha_history_stream.h"

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ha_ws.h"
#include "lvgl.h"

#define HISTORY_STREAM_DELIVERY_MS 50

typedef struct history_stream_message {
    struct history_stream_message *next;
    cJSON *states;
    esp_err_t err;
    bool reset;
} history_stream_message_t;

struct ha_history_stream {
    struct ha_history_stream *next;
    ha_ws_client_t *client;
    lv_timer_t *timer;
    SemaphoreHandle_t lock;
    history_stream_message_t *head;
    history_stream_message_t *tail;
    bool out_of_memory;
    ha_history_stream_cb_t callback;
    void *user_ctx;
    int subscription_id;
};

/* Open, close, and detach run with the LVGL lock held. */
static ha_history_stream_t *s_streams;

static void history_stream_queue(ha_history_stream_t *stream,
                                 const cJSON *states, esp_err_t err,
                                 bool reset)
{
    history_stream_message_t *message = calloc(1, sizeof(*message));
    if (!message) {
        xSemaphoreTake(stream->lock, portMAX_DELAY);
        stream->out_of_memory = true;
        xSemaphoreGive(stream->lock);
        return;
    }
    if (states) {
        message->states = cJSON_Duplicate(states, true);
        if (!message->states) {
            free(message);
            xSemaphoreTake(stream->lock, portMAX_DELAY);
            stream->out_of_memory = true;
            xSemaphoreGive(stream->lock);
            return;
        }
    }
    message->err = err;
    message->reset = reset;
    xSemaphoreTake(stream->lock, portMAX_DELAY);
    if (stream->tail) stream->tail->next = message;
    else stream->head = message;
    stream->tail = message;
    xSemaphoreGive(stream->lock);
}

static void history_stream_result_cb(ha_ws_client_t *client, int id,
                                      esp_err_t err, const cJSON *result,
                                      const cJSON *error, void *user_ctx)
{
    (void)client;
    (void)id;
    (void)result;
    (void)error;
    ha_history_stream_t *stream = user_ctx;
    history_stream_queue(stream, NULL, err, err == ESP_OK);
}

static void history_stream_event_cb(ha_ws_client_t *client, int id,
                                     const cJSON *event, void *user_ctx)
{
    (void)client;
    (void)id;
    ha_history_stream_t *stream = user_ctx;
    const cJSON *states = cJSON_GetObjectItemCaseSensitive(event, "states");
    if (cJSON_IsObject(states)) {
        history_stream_queue(stream, states, ESP_OK, false);
    }
}

static void history_stream_timer_cb(lv_timer_t *timer)
{
    ha_history_stream_t *stream = lv_timer_get_user_data(timer);
    xSemaphoreTake(stream->lock, portMAX_DELAY);
    history_stream_message_t *message = stream->head;
    const bool out_of_memory = stream->out_of_memory;
    stream->out_of_memory = false;
    if (message) {
        stream->head = message->next;
        if (!stream->head) stream->tail = NULL;
    }
    xSemaphoreGive(stream->lock);
    if (!message && !out_of_memory) return;

    /* A callback may close the stream while refreshing its More Info screen. */
    const ha_history_stream_cb_t callback = stream->callback;
    const int subscription_id = stream->subscription_id;
    void *user_ctx = stream->user_ctx;
    if (out_of_memory) {
        callback(subscription_id, ESP_ERR_NO_MEM, NULL, false, user_ctx);
        if (message) {
            cJSON_Delete(message->states);
            free(message);
        }
        return;
    }
    if (message) {
        callback(subscription_id, message->err, message->states,
                 message->reset, user_ctx);
        cJSON_Delete(message->states);
        free(message);
    }
}

esp_err_t ha_history_stream_open(ha_ws_client_t *client,
                                 const char *entity_id,
                                 unsigned hours_to_show,
                                 ha_history_stream_cb_t callback,
                                 void *user_ctx,
                                 ha_history_stream_t **stream_out,
                                 int *subscription_id_out)
{
    if (!client || !entity_id || !entity_id[0] || !hours_to_show ||
        !callback || !stream_out) return ESP_ERR_INVALID_ARG;
    *stream_out = NULL;
    ha_history_stream_t *stream = calloc(1, sizeof(*stream));
    if (!stream) return ESP_ERR_NO_MEM;
    stream->lock = xSemaphoreCreateMutex();
    if (!stream->lock) {
        free(stream);
        return ESP_ERR_NO_MEM;
    }
    stream->client = client;
    stream->callback = callback;
    stream->user_ctx = user_ctx;
    stream->timer = lv_timer_create(history_stream_timer_cb,
                                    HISTORY_STREAM_DELIVERY_MS, stream);
    if (!stream->timer) {
        vSemaphoreDelete(stream->lock);
        free(stream);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = ha_ws_subscribe_history_stream(
        client, entity_id, hours_to_show, history_stream_event_cb,
        history_stream_result_cb, stream, &stream->subscription_id);
    if (err != ESP_OK) {
        lv_timer_delete(stream->timer);
        vSemaphoreDelete(stream->lock);
        free(stream);
        return err;
    }
    if (subscription_id_out) *subscription_id_out = stream->subscription_id;
    stream->next = s_streams;
    s_streams = stream;
    *stream_out = stream;
    return ESP_OK;
}

void ha_history_stream_close(ha_history_stream_t *stream)
{
    if (!stream) return;
    ha_history_stream_t **slot = &s_streams;
    while (*slot && *slot != stream) slot = &(*slot)->next;
    if (*slot) *slot = stream->next;
    lv_timer_delete(stream->timer);
    /* Unsubscribe serializes with in-flight WebSocket callbacks. */
    if (stream->client) {
        ha_ws_unsubscribe_events_by_callback(stream->client,
                                             history_stream_event_cb, stream);
    }
    xSemaphoreTake(stream->lock, portMAX_DELAY);
    history_stream_message_t *message = stream->head;
    stream->head = NULL;
    stream->tail = NULL;
    xSemaphoreGive(stream->lock);
    while (message) {
        history_stream_message_t *next = message->next;
        cJSON_Delete(message->states);
        free(message);
        message = next;
    }
    vSemaphoreDelete(stream->lock);
    free(stream);
}

void ha_history_stream_detach_client(ha_ws_client_t *client)
{
    if (!client) return;
    for (ha_history_stream_t *stream = s_streams; stream;
         stream = stream->next) {
        if (stream->client != client) continue;
        ha_ws_unsubscribe_events_by_callback(client,
                                             history_stream_event_cb, stream);
        stream->client = NULL;
        history_stream_queue(stream, NULL, ESP_ERR_INVALID_STATE, false);
    }
}

bool ha_history_stream_is_attached(const ha_history_stream_t *stream)
{
    return stream && stream->client != NULL;
}
