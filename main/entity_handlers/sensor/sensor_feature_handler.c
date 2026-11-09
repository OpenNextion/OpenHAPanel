#include "sensor_feature_handler.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "common/hui_trend_graph/hui_trend_graph.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "sensor_entity_handler.h"

#define SENSOR_TREND_DEFAULT_HOURS 24
#define SENSOR_TREND_REQUEST_RETRY_SECONDS 60
#define SENSOR_TREND_BUCKETS 46

static const char *TAG = "sensor_feature_handler";

typedef struct sensor_trend_request sensor_trend_request_t;

typedef struct {
    lv_obj_t *graph;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    hui_trend_graph_point_t points[HUI_TREND_GRAPH_MAX_POINTS];
    size_t count;
    time_t last_epoch;
    double last_value;
    time_t last_attempt;
    int hours;
    bool last_value_valid;
    bool loaded;
    sensor_trend_request_t *request;
} sensor_trend_state_t;

struct sensor_trend_request {
    sensor_trend_state_t *state; /* Cleared before the Feature host is deleted. */
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    hui_trend_graph_point_t points[HUI_TREND_GRAPH_MAX_POINTS];
    size_t count;
    time_t start_time;
    time_t end_time;
    int request_id;
};

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

typedef struct {
    bool used;
    hui_trend_graph_point_t minimum;
    hui_trend_graph_point_t maximum;
    size_t minimum_order;
    size_t maximum_order;
} sensor_trend_bucket_t;

static void append_unique(sensor_trend_request_t *request,
                          hui_trend_graph_point_t point)
{
    if (request->count &&
        request->points[request->count - 1].epoch == point.epoch &&
        request->points[request->count - 1].value == point.value) {
        return;
    }
    if (request->count < HUI_TREND_GRAPH_MAX_POINTS - 1) {
        request->points[request->count++] = point;
    }
}

static bool parse_history(sensor_trend_request_t *request, const cJSON *result)
{
    if (!cJSON_IsObject(result)) {
        return false;
    }
    const cJSON *states = cJSON_GetObjectItemCaseSensitive(result,
                                                            request->entity_id);
    if (!states && !result->child) {
        return true;
    }
    if (!cJSON_IsArray(states)) {
        return false;
    }
    sensor_trend_bucket_t buckets[SENSOR_TREND_BUCKETS] = {0};
    hui_trend_graph_point_t first = {0};
    hui_trend_graph_point_t last = {0};
    size_t valid_count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, states) {
        hui_trend_graph_point_t point;
        if (parse_history_point(item, &point) &&
            point.epoch >= request->start_time &&
            point.epoch <= request->end_time) {
            if (!valid_count) {
                first = point;
            }
            if (valid_count < SENSOR_TREND_BUCKETS) {
                request->points[valid_count] = point;
            }
            last = point;
            size_t bucket_index = (size_t)(
                ((int64_t)(point.epoch - request->start_time) *
                 SENSOR_TREND_BUCKETS) /
                (request->end_time - request->start_time));
            if (bucket_index >= SENSOR_TREND_BUCKETS) {
                bucket_index = SENSOR_TREND_BUCKETS - 1;
            }
            sensor_trend_bucket_t *bucket = &buckets[bucket_index];
            if (!bucket->used) {
                bucket->used = true;
                bucket->minimum = point;
                bucket->maximum = point;
                bucket->minimum_order = valid_count;
                bucket->maximum_order = valid_count;
            } else {
                if (point.value < bucket->minimum.value) {
                    bucket->minimum = point;
                    bucket->minimum_order = valid_count;
                }
                if (point.value > bucket->maximum.value) {
                    bucket->maximum = point;
                    bucket->maximum_order = valid_count;
                }
            }
            valid_count++;
        }
    }
    if (!valid_count) {
        return true;
    }
    /* Home Assistant keeps the original states when they already fit within
     * the detail budget. Extremum buckets would discard intermediate changes
     * even for a short history such as 20 -> 50 -> 70 -> 80 -> 23. */
    if (valid_count <= SENSOR_TREND_BUCKETS) {
        request->count = valid_count;
        return true;
    }
    append_unique(request, first);
    for (size_t i = 0; i < SENSOR_TREND_BUCKETS; i++) {
        const sensor_trend_bucket_t *bucket = &buckets[i];
        if (!bucket->used) {
            continue;
        }
        if (bucket->minimum_order <= bucket->maximum_order) {
            append_unique(request, bucket->minimum);
            append_unique(request, bucket->maximum);
        } else {
            append_unique(request, bucket->maximum);
            append_unique(request, bucket->minimum);
        }
    }
    append_unique(request, last);
    return true;
}

static void append_live_point(sensor_trend_state_t *state,
                              hui_trend_graph_point_t point)
{
    if (state->count) {
        const hui_trend_graph_point_t *last = &state->points[state->count - 1];
        if (point.epoch < last->epoch ||
            (point.epoch == last->epoch && point.value == last->value)) {
            return;
        }
    }
    if (state->count == HUI_TREND_GRAPH_MAX_POINTS) {
        memmove(state->points, state->points + 1,
                (state->count - 1) * sizeof(state->points[0]));
        state->count--;
    }
    state->points[state->count++] = point;
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
    size_t first = 0;
    while (first < state->count && state->points[first].epoch < start) {
        first++;
    }
    if (first) {
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
        return;
    }
    hui_trend_graph_set_points(state->graph, state->points, state->count,
                               start, now, animate);
}

static void history_result_cb(int request_id, esp_err_t err,
                              const cJSON *result, const cJSON *error,
                              void *user_ctx)
{
    (void)request_id;
    (void)error;
    sensor_trend_request_t *request = user_ctx;
    if (!request) {
        return;
    }
    const bool valid = err == ESP_OK && parse_history(request, result);
    lvgl_port_lock(0);
    sensor_trend_state_t *state = request->state;
    if (state && state->request == request) {
        state->request = NULL;
        state->loaded = valid;
        if (valid) {
            state->count = request->count;
            memcpy(state->points, request->points,
                   state->count * sizeof(state->points[0]));
            if (state->last_value_valid) {
                const hui_trend_graph_point_t live = {
                    .epoch = state->last_epoch,
                    .value = state->last_value,
                };
                append_live_point(state, live);
            }
            render_graph(state, true);
        } else {
            ESP_LOGW(TAG, "History request for %s failed: %s",
                     request->entity_id,
                     err == ESP_OK ? "invalid response" : esp_err_to_name(err));
        }
    }
    lvgl_port_unlock();
    free(request);
}

static void request_history(sensor_trend_state_t *state)
{
    if (!state || state->loaded || state->request) {
        return;
    }
    const time_t now = time(NULL);
    if (now <= 0 ||
        (state->last_attempt &&
         now - state->last_attempt < SENSOR_TREND_REQUEST_RETRY_SECONDS)) {
        return;
    }
    state->last_attempt = now;
    sensor_trend_request_t *request = calloc(1, sizeof(*request));
    if (!request) {
        return;
    }
    request->state = state;
    request->start_time = now - state->hours * 60 * 60;
    request->end_time = now;
    snprintf(request->entity_id, sizeof(request->entity_id), "%s",
             state->entity_id);
    state->request = request;
    const esp_err_t err = app_ha_get_entity_history(
        state->entity_id, request->start_time, request->end_time,
        history_result_cb, request, &request->request_id);
    if (err != ESP_OK) {
        state->request = NULL;
        free(request);
        ESP_LOGW(TAG, "Cannot request history for %s: %s",
                 state->entity_id, esp_err_to_name(err));
    }
}

static void feature_delete_cb(lv_event_t *event)
{
    sensor_trend_state_t *state = lv_event_get_user_data(event);
    if (!state || lv_event_get_target_obj(event) !=
                      lv_event_get_current_target_obj(event)) {
        return;
    }
    if (state->request) {
        state->request->state = NULL;
    }
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
    state->hours = SENSOR_TREND_DEFAULT_HOURS;
    const cJSON *hours = instance->config->data
        ? cJSON_GetObjectItemCaseSensitive(instance->config->data, "hours") : NULL;
    if (cJSON_IsNumber(hours) && hours->valueint >= 1 && hours->valueint <= 168) {
        state->hours = hours->valueint;
    }
    snprintf(state->entity_id, sizeof(state->entity_id), "%s",
             instance->entity_id);
    state->graph = hui_trend_graph_create(instance->host);
    if (!state->graph) {
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
        render_graph(state, true);
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
            append_live_point(state,
                              (hui_trend_graph_point_t){ .epoch = epoch,
                                                        .value = value });
            render_graph(state, true);
        }
    }
    request_history(state);
}

const entity_feature_handler_t sensor_feature_handler = {
    .supports = sensor_feature_supports,
    .create = sensor_feature_create,
    .update = sensor_feature_update,
};
