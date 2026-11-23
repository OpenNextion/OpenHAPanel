#include "sensor_feature_handler.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "common/hui_trend_graph/hui_trend_graph.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "ha_ws.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sensor_entity_handler.h"

#define SENSOR_TREND_DEFAULT_HOURS 24
#define SENSOR_TREND_REQUEST_RETRY_SECONDS 60
#define SENSOR_TREND_RETRY_POLL_MS 5000
#define SENSOR_TREND_RENDER_DELAY_MS 200
#define SENSOR_TREND_MIN_VALID_EPOCH 1577836800 /* 2020-01-01 UTC */

static const char *TAG = "sensor_feature_handler";

typedef struct {
    lv_obj_t *graph;
    lv_timer_t *retry_timer;
    lv_timer_t *render_timer;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    hui_trend_graph_point_t *points;
    size_t count;
    size_t capacity;
    hui_trend_graph_point_t *sampled;
    SemaphoreHandle_t incoming_lock;
    hui_trend_graph_point_t *incoming;
    size_t incoming_count;
    size_t incoming_capacity;
    time_t last_epoch;
    double last_value;
    int64_t last_attempt_us;
    int64_t last_render_us;
    time_t last_render_minute;
    int32_t last_graph_width;
    int hours;
    bool detail;
    bool last_value_valid;
    bool loaded;
    bool subscribed;
    bool stream_failed;
    bool incoming_reset;
    bool incoming_event;
    bool incoming_overflow;
    bool fallback_dirty;
} sensor_trend_state_t;

static bool parse_number(const char *text, double *value)
{
    if (!text || !*text || !value) {
        return false;
    }
    char *end = NULL;
    const double parsed = strtod(text, &end);
    while (end != text && *end && isspace((unsigned char)*end)) {
        end++;
    }
    if (end == text || *end || !isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_history_point(const cJSON *item,
                                hui_trend_graph_point_t *point)
{
    if (!cJSON_IsObject(item) || !point) {
        return false;
    }
    const cJSON *state = cJSON_GetObjectItemCaseSensitive(item, "s");
    if (!cJSON_IsString(state)) {
        state = cJSON_GetObjectItemCaseSensitive(item, "state");
    }
    if (!cJSON_IsString(state) ||
        !parse_number(state->valuestring, &point->value)) {
        return false;
    }
    const cJSON *updated = cJSON_GetObjectItemCaseSensitive(item, "lu");
    if (cJSON_IsNumber(updated) && updated->valuedouble > 0.0) {
        point->epoch = (time_t)updated->valuedouble;
    } else {
        updated = cJSON_GetObjectItemCaseSensitive(item, "last_updated");
        if (!cJSON_IsString(updated) || !updated->valuestring ||
            ha_datetime_parse_epoch(updated->valuestring, &point->epoch) != ESP_OK) {
            return false;
        }
    }
    return point->epoch > 0;
}

static bool append_incoming_point(sensor_trend_state_t *state,
                                  hui_trend_graph_point_t point)
{
    if (state->incoming_count == state->incoming_capacity) {
        size_t capacity = state->incoming_capacity > 16
            ? state->incoming_capacity * 2 : 32;
        if (capacity < state->incoming_capacity ||
            capacity > SIZE_MAX / sizeof(*state->incoming)) return false;
        hui_trend_graph_point_t *points = heap_caps_realloc(
            state->incoming, capacity * sizeof(*points),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!points) return false;
        state->incoming = points;
        state->incoming_capacity = capacity;
    }
    state->incoming[state->incoming_count++] = point;
    return true;
}

static void trend_stream_result_cb(ha_ws_client_t *client, int id,
                                   esp_err_t err, const cJSON *result,
                                   const cJSON *error, void *user_ctx)
{
    (void)id; (void)result; (void)error;
    sensor_trend_state_t *state = user_ctx;
    xSemaphoreTake(state->incoming_lock, portMAX_DELAY);
    if (err == ESP_OK) {
        state->incoming_count = 0;
        state->incoming_reset = true;
        state->incoming_event = false;
        state->incoming_overflow = false;
        state->stream_failed = false;
    } else {
        state->stream_failed = ha_ws_is_authenticated(client);
    }
    xSemaphoreGive(state->incoming_lock);
}

static void trend_stream_event_cb(ha_ws_client_t *client, int id,
                                  const cJSON *event, void *user_ctx)
{
    (void)client; (void)id;
    sensor_trend_state_t *state = user_ctx;
    const cJSON *states = cJSON_GetObjectItemCaseSensitive(event, "states");
    if (!cJSON_IsObject(states)) return;
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(
        states, state->entity_id);
    if (rows && !cJSON_IsArray(rows)) return;
    xSemaphoreTake(state->incoming_lock, portMAX_DELAY);
    if (rows) {
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, rows) {
            hui_trend_graph_point_t point;
            if (parse_history_point(item, &point) &&
                !append_incoming_point(state, point)) {
                state->incoming_overflow = true;
                break;
            }
        }
    }
    state->incoming_event = true;
    xSemaphoreGive(state->incoming_lock);
}

static bool append_live_point(sensor_trend_state_t *state,
                              hui_trend_graph_point_t point)
{
    size_t pos = state->count;
    while (pos > 0 && state->points[pos - 1].epoch > point.epoch) pos--;
    if ((pos > 0 && state->points[pos - 1].epoch == point.epoch &&
         state->points[pos - 1].value == point.value) ||
        (pos < state->count && state->points[pos].epoch == point.epoch &&
         state->points[pos].value == point.value)) {
        return true;
    }
    if (state->count == state->capacity) {
        size_t capacity = state->capacity > 16 ? state->capacity * 2 : 32;
        if (capacity < state->capacity ||
            capacity > SIZE_MAX / sizeof(*state->points)) {
            return false;
        }
        hui_trend_graph_point_t *points = heap_caps_realloc(
            state->points, capacity * sizeof(*state->points),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!points) {
            return false;
        }
        state->points = points;
        state->capacity = capacity;
    }
    if (pos < state->count) {
        memmove(state->points + pos + 1, state->points + pos,
                (state->count - pos) * sizeof(*state->points));
    }
    state->points[pos] = point;
    state->count++;
    return true;
}

static time_t trend_frame_size(time_t desired)
{
    static const time_t sizes[] = {
        1, 2, 3, 5, 10, 15, 20, 30,
        60, 120, 180, 300, 600, 900, 1200, 1800,
        3600, 7200, 10800, 14400, 21600, 28800, 43200, 86400,
    };
    if (desired >= 86400) {
        return (desired / 86400) * 86400;
    }
    time_t step = 1;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        if (sizes[i] > desired) {
            break;
        }
        step = sizes[i];
    }
    return step;
}

static size_t sample_trend_points(sensor_trend_state_t *state,
                                  time_t start, time_t now)
{
    lv_obj_update_layout(state->graph);
    const int32_t width = lv_obj_get_width(state->graph);
    size_t details = state->detail
        ? (size_t)LV_MAX(LV_MAX(10, width / 5), state->hours)
        : (size_t)LV_MAX(10, state->hours);
    if (state->count <= details &&
        state->count <= HUI_TREND_GRAPH_MAX_POINTS) {
        const size_t count = state->count;
        if (count) {
            memcpy(state->sampled, state->points,
                   count * sizeof(*state->sampled));
        }
        return count;
    }
    const time_t span = now - start;
    const time_t desired = (span + (time_t)details - 1) / (time_t)details;
    time_t step = trend_frame_size(LV_MAX(1, desired));
    for (;;) {
        size_t output = 0;
        bool overflow = false;
        for (size_t first = 0; first < state->count;) {
            const int64_t frame = (int64_t)state->points[first].epoch / step;
            size_t end = first + 1;
            while (end < state->count &&
                   (int64_t)state->points[end].epoch / step == frame) {
                end++;
            }
            if (state->detail) {
                size_t min_i = first;
                size_t max_i = first;
                for (size_t i = first + 1; i < end; i++) {
                    if (state->points[i].value < state->points[min_i].value) {
                        min_i = i;
                    }
                    if (state->points[i].value > state->points[max_i].value) {
                        max_i = i;
                    }
                }
                if (output + (min_i == max_i ? 1 : 2) >
                    HUI_TREND_GRAPH_MAX_POINTS) {
                    overflow = true;
                    break;
                }
                if (min_i < max_i) {
                    state->sampled[output++] = state->points[min_i];
                    state->sampled[output++] = state->points[max_i];
                } else if (max_i < min_i) {
                    state->sampled[output++] = state->points[max_i];
                    state->sampled[output++] = state->points[min_i];
                } else {
                    state->sampled[output++] = state->points[min_i];
                }
            } else {
                if (output == HUI_TREND_GRAPH_MAX_POINTS) {
                    overflow = true;
                    break;
                }
                double sum_time = 0.0;
                double sum_value = 0.0;
                for (size_t i = first; i < end; i++) {
                    sum_time += (double)state->points[i].epoch;
                    sum_value += state->points[i].value;
                }
                state->sampled[output++] = (hui_trend_graph_point_t){
                    .epoch = (time_t)llround(sum_time / (end - first)),
                    .value = sum_value / (end - first),
                };
            }
            first = end;
        }
        if (!overflow) {
            return output;
        }
        if (step > span) {
            return 0;
        }
        step *= 2;
    }
}

static void render_graph(sensor_trend_state_t *state, bool animate)
{
    if (!state || !state->graph) {
        return;
    }
    const time_t now = time(NULL);
    if (now <= 0) {
        return;
    }
    const time_t start = now - state->hours * 60 * 60;
    state->last_render_us = esp_timer_get_time();
    size_t first = 0;
    while (first < state->count && state->points[first].epoch < start) {
        first++;
    }
    if (first) {
        /* Keep the value active at the moving window boundary. */
        if (first == state->count || state->points[first].epoch > start) {
            first--;
            state->points[first].epoch = start;
        }
        state->count -= first;
        memmove(state->points, state->points + first,
                state->count * sizeof(state->points[0]));
    }
    if (state->count == 0 && state->last_value_valid) {
        /* A stable sensor can have no recorder entries in this period. Draw
         * its current value as one point without changing the history cache. */
        const hui_trend_graph_point_t current = {
            .epoch = now,
            .value = state->last_value,
        };
        hui_trend_graph_set_points(state->graph, &current, 1, start, now,
                                   animate);
        state->last_graph_width = lv_obj_get_width(state->graph);
        state->last_render_minute = now / 60;
        return;
    }
    const size_t count = sample_trend_points(state, start, now);
    hui_trend_graph_set_points(state->graph, state->sampled, count,
                               start, now, animate);
    state->last_graph_width = lv_obj_get_width(state->graph);
    state->last_render_minute = now / 60;
}

static bool consume_stream_points(sensor_trend_state_t *state)
{
    xSemaphoreTake(state->incoming_lock, portMAX_DELAY);
    const bool reset = state->incoming_reset;
    const bool event = state->incoming_event;
    const bool overflow = state->incoming_overflow;
    hui_trend_graph_point_t *incoming = state->incoming;
    const size_t count = state->incoming_count;
    state->incoming = NULL;
    state->incoming_count = 0;
    state->incoming_capacity = 0;
    state->incoming_reset = false;
    state->incoming_event = false;
    state->incoming_overflow = false;
    xSemaphoreGive(state->incoming_lock);
    if (reset) {
        heap_caps_free(state->points);
        state->points = NULL;
        state->count = 0;
        state->capacity = 0;
        state->loaded = false;
    }
    bool append_failed = false;
    for (size_t i = 0; i < count; i++) {
        if (!append_live_point(state, incoming[i])) {
            append_failed = true;
            break;
        }
    }
    heap_caps_free(incoming);
    const bool first_event = event && !state->loaded;
    if (event) {
        state->loaded = true;
    }
    if (overflow || append_failed) {
        xSemaphoreTake(state->incoming_lock, portMAX_DELAY);
        state->stream_failed = true;
        xSemaphoreGive(state->incoming_lock);
    }
    return reset || first_event || count > 0;
}

static void subscribe_history(sensor_trend_state_t *state)
{
    if (!state || !app_ha_trend_history_connected()) return;
    const time_t now = time(NULL);
    if (now < SENSOR_TREND_MIN_VALID_EPOCH ||
        now <= (time_t)state->hours * 60 * 60) return;
    xSemaphoreTake(state->incoming_lock, portMAX_DELAY);
    const bool failed = state->stream_failed;
    xSemaphoreGive(state->incoming_lock);
    if (state->subscribed && !failed) return;
    const int64_t attempt_us = esp_timer_get_time();
    if (state->last_attempt_us > 0 &&
        attempt_us - state->last_attempt_us <
            (int64_t)SENSOR_TREND_REQUEST_RETRY_SECONDS * 1000000) return;
    if (state->subscribed) {
        app_ha_unsubscribe_trend_history(trend_stream_event_cb, state);
        state->subscribed = false;
    }
    state->last_attempt_us = attempt_us;
    const esp_err_t err = app_ha_subscribe_trend_history(
        state->entity_id, (unsigned)state->hours,
        trend_stream_event_cb, trend_stream_result_cb, state, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot subscribe to history for %s: %s",
                 state->entity_id, esp_err_to_name(err));
    } else {
        state->subscribed = true;
        xSemaphoreTake(state->incoming_lock, portMAX_DELAY);
        state->stream_failed = false;
        xSemaphoreGive(state->incoming_lock);
    }
}

static void sensor_trend_retry_timer_cb(lv_timer_t *timer)
{
    sensor_trend_state_t *state = lv_timer_get_user_data(timer);
    const time_t now = time(NULL);
    if (state && state->loaded && now > 0 &&
        (state->last_render_minute != now / 60 ||
         state->last_graph_width != lv_obj_get_width(state->graph))) {
        render_graph(state, false);
    }
    subscribe_history(state);
}

static void sensor_trend_render_timer_cb(lv_timer_t *timer)
{
    sensor_trend_state_t *state = lv_timer_get_user_data(timer);
    if (!state) return;
    const bool changed = consume_stream_points(state);
    if (!changed && !state->fallback_dirty) return;
    state->fallback_dirty = false;
    const int64_t now_us = esp_timer_get_time();
    const bool animate = state->last_render_us == 0 ||
        now_us - state->last_render_us >= 500000;
    render_graph(state, animate);
}

static void feature_delete_cb(lv_event_t *event)
{
    sensor_trend_state_t *state = lv_event_get_user_data(event);
    if (!state || lv_event_get_target_obj(event) !=
                      lv_event_get_current_target_obj(event)) {
        return;
    }
    if (state->subscribed) {
        app_ha_unsubscribe_trend_history(trend_stream_event_cb, state);
    }
    if (state->retry_timer) {
        lv_timer_delete(state->retry_timer);
    }
    if (state->render_timer) {
        lv_timer_delete(state->render_timer);
    }
    heap_caps_free(state->points);
    heap_caps_free(state->sampled);
    heap_caps_free(state->incoming);
    vSemaphoreDelete(state->incoming_lock);
    lv_obj_set_user_data(lv_event_get_target_obj(event), NULL);
    lv_free(state);
}

static bool sensor_feature_supports(ha_entity_store_t *store,
                                    const char *entity_id,
                                    screen_feature_type_t type)
{
    if (type != SCREEN_FEATURE_TREND_GRAPH) {
        return false;
    }
    if (!store || !entity_id) {
        return false;
    }
    ha_entity_snapshot_t snapshot = {0};
    if (ha_entity_store_get(store, entity_id, &snapshot) != ESP_OK) {
        return false;
    }
    return sensor_entity_has_chart_history(&snapshot);
}

static esp_err_t sensor_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config ||
        instance->config->type != SCREEN_FEATURE_TREND_GRAPH) {
        return ESP_ERR_INVALID_ARG;
    }
    sensor_trend_state_t *state = lv_malloc(sizeof(*state));
    if (!state) {
        return ESP_ERR_NO_MEM;
    }
    memset(state, 0, sizeof(*state));
    state->incoming_lock = xSemaphoreCreateMutex();
    if (!state->incoming_lock) {
        lv_free(state);
        return ESP_ERR_NO_MEM;
    }
    state->hours = SENSOR_TREND_DEFAULT_HOURS;
    state->detail = true;
    const cJSON *hours = instance->config->data
        ? cJSON_GetObjectItemCaseSensitive(instance->config->data,
                                           "hours_to_show") : NULL;
    if (!hours && instance->config->data) {
        hours = cJSON_GetObjectItemCaseSensitive(instance->config->data,
                                                  "hours");
    }
    if (cJSON_IsNumber(hours) && hours->valueint >= 1 && hours->valueint <= 168) {
        state->hours = hours->valueint;
    }
    const cJSON *detail = instance->config->data
        ? cJSON_GetObjectItemCaseSensitive(instance->config->data, "detail") : NULL;
    if (cJSON_IsBool(detail)) {
        state->detail = cJSON_IsTrue(detail);
    }
    state->sampled = heap_caps_malloc(
        HUI_TREND_GRAPH_MAX_POINTS * sizeof(*state->sampled),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!state->sampled) {
        vSemaphoreDelete(state->incoming_lock);
        lv_free(state);
        return ESP_ERR_NO_MEM;
    }
    snprintf(state->entity_id, sizeof(state->entity_id), "%s",
             instance->entity_id);
    state->graph = hui_trend_graph_create(instance->host);
    if (!state->graph) {
        heap_caps_free(state->sampled);
        vSemaphoreDelete(state->incoming_lock);
        lv_free(state);
        return ESP_ERR_NO_MEM;
    }
    /* Poll for clock readiness and retry failed requests even when the sensor
     * state does not change. Spread timers by entity to avoid a request burst. */
    uint32_t phase = 0;
    for (const char *p = state->entity_id; *p; p++) {
        phase = phase * 33U + (uint8_t)*p;
    }
    state->retry_timer = lv_timer_create(
        sensor_trend_retry_timer_cb,
        SENSOR_TREND_RETRY_POLL_MS + phase % SENSOR_TREND_RETRY_POLL_MS,
        state);
    if (!state->retry_timer) {
        lv_obj_delete(state->graph);
        heap_caps_free(state->sampled);
        vSemaphoreDelete(state->incoming_lock);
        lv_free(state);
        return ESP_ERR_NO_MEM;
    }
    state->render_timer = lv_timer_create(sensor_trend_render_timer_cb,
                                          SENSOR_TREND_RENDER_DELAY_MS, state);
    if (!state->render_timer) {
        lv_timer_delete(state->retry_timer);
        lv_obj_delete(state->graph);
        heap_caps_free(state->sampled);
        vSemaphoreDelete(state->incoming_lock);
        lv_free(state);
        return ESP_ERR_NO_MEM;
    }
    instance->control = state->graph;
    lv_obj_set_user_data(instance->host, state);
    lv_obj_add_event_cb(instance->host, feature_delete_cb,
                        LV_EVENT_DELETE, state);
    return ESP_OK;
}

static void sensor_feature_update(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    (void)store;
    if (!instance || !instance->control || !snapshot) {
        return;
    }
    sensor_trend_state_t *state = lv_obj_get_user_data(instance->host);
    if (!state) {
        return;
    }
    hui_trend_graph_set_color(
        state->graph,
        lv_color_hex(sensor_entity_icon_color(snapshot)));
    double value;
    const bool current_available =
        !entity_handler_snapshot_is_unavailable(snapshot) &&
        parse_number(snapshot->state, &value);
    if (!current_available && state->last_value_valid) {
        state->last_value_valid = false;
        state->fallback_dirty = true;
    }
    if (current_available) {
        time_t epoch = time(NULL);
        if (snapshot->last_updated[0]) {
            time_t updated = 0;
            if (ha_datetime_parse_epoch(snapshot->last_updated, &updated) == ESP_OK &&
                updated > 0) {
                epoch = updated;
            }
        }
        if (epoch > 0 &&
            (!state->last_value_valid || state->last_epoch != epoch ||
             fabs(state->last_value - value) > 0.000001)) {
            state->last_epoch = epoch;
            state->last_value = value;
            state->last_value_valid = true;
            if (!state->loaded || state->count == 0) {
                state->fallback_dirty = true;
            }
        }
    }
    subscribe_history(state);
}

static void sensor_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (instance && instance->control) {
        hui_trend_graph_apply_theme(instance->control);
    }
}

const entity_feature_handler_t sensor_feature_handler = {
    .supports = sensor_feature_supports,
    .create = sensor_feature_create,
    .update = sensor_feature_update,
    .apply_theme = sensor_feature_apply_theme,
};
