#include "../entity_state_translation.h"
#include "ui_i18n.h"
#include "i18n.h"
#include "sensor_entity_handler.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "screen/components/ha_history_chart.h"
#include "screen/components/ha_history_list.h"
#include "screen/components/ha_relative_time_label.h"
#include "screen/components/more_info_screen.h"
#include "bsp_lcd.h"
#include "sys_time_api.h"

#define SCREEN_DEFAULT_CONTENT_WIDTH_PORTAIT 280
#define SCREEN_DEFAULT_CONTENT_WIDTH_LANDSCAPE 440
#define SENSOR_TITLE_MIN_WIDTH_LANDSCAPE 120
#define SENSOR_TITLE_STATE_GAP_LANDSCAPE 8
#define SENSOR_HISTORY_HOURS 12
#define SENSOR_HISTORY_LIST_DAYS 3
#define SENSOR_HISTORY_LIST_MAX_RECORDS 14

static const char *TAG = "sensor_entity_handler";

typedef ha_history_chart_point_t sensor_history_point_t;

typedef struct {
    time_t epoch;
    char state[HA_ENTITY_STORE_STATE_MAX];
} sensor_history_record_t;

typedef struct {
    lv_obj_t *screen;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    char device_class[HA_ENTITY_STORE_DEVICE_CLASS_MAX];
    char unit[HA_ENTITY_STORE_UNIT_MAX];
    time_t start_time;
    time_t end_time;
    int request_id;
    bool loading;
    bool loaded;
    bool failed;
    bool is_list_history;
    bool observed_state_initialized;
    char observed_state[HA_ENTITY_STORE_STATE_MAX];
    size_t point_count;
    size_t point_capacity;
    sensor_history_point_t *points;
    size_t record_count;
    sensor_history_record_t records[SENSOR_HISTORY_LIST_MAX_RECORDS];
} sensor_history_cache_t;

static sensor_history_cache_t s_sensor_history;

typedef struct {
    const char *device_class;
    const char *icon;
} sensor_icon_mapping_t;

/* Default device-class icons from Home Assistant sensor/icons.json.
 * The frontend resolves these through src/data/icons.ts. */
static const sensor_icon_mapping_t s_sensor_icons[] = {
    {"absolute_humidity", MDI_SYMBOL_WATER_OPACITY},
    {"apparent_power", MDI_SYMBOL_FLASH},
    {"aqi", MDI_SYMBOL_AIR_FILTER},
    {"area", MDI_SYMBOL_TEXTURE_BOX},
    {"atmospheric_pressure", MDI_SYMBOL_THERMOMETER_LINES},
    {"battery", MDI_SYMBOL_BATTERY_UNKNOWN},
    {"blood_glucose_concentration", MDI_SYMBOL_SPOON_SUGAR},
    {"carbon_dioxide", MDI_SYMBOL_MOLECULE_CO2},
    {"carbon_monoxide", MDI_SYMBOL_MOLECULE_CO},
    {"conductivity", MDI_SYMBOL_SPROUT_OUTLINE},
    {"current", MDI_SYMBOL_CURRENT_AC},
    {"data_rate", MDI_SYMBOL_TRANSMISSION_TOWER},
    {"data_size", MDI_SYMBOL_DATABASE},
    {"date", MDI_SYMBOL_CALENDAR},
    {"distance", MDI_SYMBOL_ARROW_LEFT_RIGHT},
    {"duration", MDI_SYMBOL_PROGRESS_CLOCK},
    {"energy", MDI_SYMBOL_LIGHTNING_BOLT},
    {"energy_storage", MDI_SYMBOL_CAR_BATTERY},
    {"enum", MDI_SYMBOL_EYE},
    {"frequency", MDI_SYMBOL_SINE_WAVE},
    {"gas", MDI_SYMBOL_METER_GAS},
    {"humidity", MDI_SYMBOL_WATER_PERCENT},
    {"illuminance", MDI_SYMBOL_BRIGHTNESS_5},
    {"irradiance", MDI_SYMBOL_SUN_WIRELESS},
    {"moisture", MDI_SYMBOL_WATER_PERCENT},
    {"monetary", MDI_SYMBOL_CASH},
    {"nitrogen_dioxide", MDI_SYMBOL_MOLECULE},
    {"nitrogen_monoxide", MDI_SYMBOL_MOLECULE},
    {"nitrous_oxide", MDI_SYMBOL_MOLECULE},
    {"ozone", MDI_SYMBOL_MOLECULE},
    {"ph", MDI_SYMBOL_PH},
    {"pm1", MDI_SYMBOL_MOLECULE},
    {"pm10", MDI_SYMBOL_MOLECULE},
    {"pm25", MDI_SYMBOL_MOLECULE},
    {"pm4", MDI_SYMBOL_MOLECULE},
    {"power", MDI_SYMBOL_FLASH},
    {"power_factor", MDI_SYMBOL_ANGLE_ACUTE},
    {"precipitation", MDI_SYMBOL_WEATHER_RAINY},
    {"precipitation_intensity", MDI_SYMBOL_WEATHER_POURING},
    {"pressure", MDI_SYMBOL_GAUGE},
    {"radon", MDI_SYMBOL_RADIOACTIVE},
    {"reactive_energy", MDI_SYMBOL_LIGHTNING_BOLT},
    {"reactive_power", MDI_SYMBOL_FLASH},
    {"signal_strength", MDI_SYMBOL_WIFI},
    {"sound_pressure", MDI_SYMBOL_EAR_HEARING},
    {"speed", MDI_SYMBOL_SPEEDOMETER},
    {"sulphur_dioxide", MDI_SYMBOL_MOLECULE},
    {"temperature", MDI_SYMBOL_THERMOMETER},
    {"temperature_delta", MDI_SYMBOL_THERMOMETER},
    {"timestamp", MDI_SYMBOL_CLOCK},
    {"uptime", MDI_SYMBOL_CLOCK_START},
    {"volatile_organic_compounds", MDI_SYMBOL_MOLECULE},
    {"volatile_organic_compounds_parts", MDI_SYMBOL_MOLECULE},
    {"voltage", MDI_SYMBOL_SINE_WAVE},
    {"volume", MDI_SYMBOL_CAR_COOLANT_LEVEL},
    {"volume_flow_rate", MDI_SYMBOL_PIPE_VALVE},
    {"volume_storage", MDI_SYMBOL_STORAGE_TANK},
    {"water", MDI_SYMBOL_WATER},
    {"weight", MDI_SYMBOL_WEIGHT},
    {"wind_direction", MDI_SYMBOL_COMPASS_ROSE},
    {"wind_speed", MDI_SYMBOL_WEATHER_WINDY},
};

typedef struct {
    const char *device_class;
    double threshold;
    const char *icon;
} sensor_range_icon_mapping_t;

/* Select the highest threshold <= state, matching getIconFromRange(). */
static const sensor_range_icon_mapping_t s_sensor_range_icons[] = {
    {"battery", 0, MDI_SYMBOL_BATTERY_ALERT},
    {"battery", 10, MDI_SYMBOL_BATTERY_10},
    {"battery", 20, MDI_SYMBOL_BATTERY_20},
    {"battery", 30, MDI_SYMBOL_BATTERY_30},
    {"battery", 40, MDI_SYMBOL_BATTERY_40},
    {"battery", 50, MDI_SYMBOL_BATTERY_50},
    {"battery", 60, MDI_SYMBOL_BATTERY_60},
    {"battery", 70, MDI_SYMBOL_BATTERY_70},
    {"battery", 80, MDI_SYMBOL_BATTERY_80},
    {"battery", 90, MDI_SYMBOL_BATTERY_90},
    {"battery", 100, MDI_SYMBOL_BATTERY},
    {"wind_direction", 0, MDI_SYMBOL_ARROW_DOWN},
    {"wind_direction", 22.5, MDI_SYMBOL_ARROW_BOTTOM_LEFT},
    {"wind_direction", 67.5, MDI_SYMBOL_ARROW_LEFT},
    {"wind_direction", 112.5, MDI_SYMBOL_ARROW_TOP_LEFT},
    {"wind_direction", 157.5, MDI_SYMBOL_ARROW_UP},
    {"wind_direction", 202.5, MDI_SYMBOL_ARROW_TOP_RIGHT},
    {"wind_direction", 247.5, MDI_SYMBOL_ARROW_RIGHT},
    {"wind_direction", 292.5, MDI_SYMBOL_ARROW_BOTTOM_RIGHT},
    {"wind_direction", 337.5, MDI_SYMBOL_ARROW_DOWN},
};

/* Home Assistant frontend's SENSOR_NUMERIC_DEVICE_CLASSES. */
static const char *const s_numeric_sensor_device_classes[] = {
    "absolute_humidity",
    "apparent_power",
    "aqi",
    "area",
    "atmospheric_pressure",
    "battery",
    "blood_glucose_concentration",
    "carbon_dioxide",
    "carbon_monoxide",
    "conductivity",
    "current",
    "data_rate",
    "data_size",
    "distance",
    "duration",
    "energy",
    "energy_distance",
    "energy_storage",
    "frequency",
    "gas",
    "humidity",
    "illuminance",
    "irradiance",
    "moisture",
    "monetary",
    "nitrogen_dioxide",
    "nitrogen_monoxide",
    "nitrous_oxide",
    "ozone",
    "ph",
    "power",
    "power_factor",
    "precipitation",
    "precipitation_intensity",
    "pressure",
    "radon",
    "reactive_energy",
    "reactive_power",
    "signal_strength",
    "sound_pressure",
    "speed",
    "sulphur_dioxide",
    "temperature",
    "temperature_delta",
    "volatile_organic_compounds",
    "volatile_organic_compounds_parts",
    "voltage",
    "volume",
    "volume_flow_rate",
    "volume_storage",
    "water",
    "weight",
    "wind_direction",
    "wind_speed",
};

static bool sensor_has_chart_history(const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot) {
        return false;
    }
    if (snapshot->has_unit_of_measurement || snapshot->has_state_class) {
        return true;
    }
    for (size_t i = 0;
         i < sizeof(s_numeric_sensor_device_classes) /
                 sizeof(s_numeric_sensor_device_classes[0]);
         i++) {
        if (strcmp(snapshot->device_class,
                   s_numeric_sensor_device_classes[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool sensor_parse_numeric_state(const char *state, double *value_out);
static bool sensor_history_parse_epoch(const cJSON *item, time_t *epoch_out);
static void sensor_history_insert_record(time_t epoch, const char *state);
static bool sensor_history_parse_list_result(const cJSON *result);
static bool sensor_history_parse_item(const cJSON *item,
                                      sensor_history_point_t *point_out);
static bool sensor_history_parse_result(const cJSON *result);
static bool sensor_history_reserve(size_t required_capacity);
static void sensor_history_prune(time_t now_epoch);
static void sensor_history_observe_state(
    const ha_entity_snapshot_t *snapshot);
static void sensor_history_prepare(
    const entity_more_info_context_t *context);
static void sensor_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx);
static void sensor_history_screen_delete_cb(lv_event_t *event);
static void sensor_history_reset(void);
static void sensor_history_render(lv_obj_t *parent);
static bool sensor_format_timestamp_state(const char *state,
                                          char *output,
                                          size_t output_size);

static bool sensor_parse_numeric_state(const char *state, double *value_out)
{
    if (!state || !state[0] || !value_out) {
        return false;
    }

    char *end = NULL;
    const double value = strtod(state, &end);
    if (end == state || !isfinite(value)) {
        return false;
    }
    while (*end && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end) {
        return false;
    }

    *value_out = value;
    return true;
}

static bool sensor_history_parse_epoch(const cJSON *item, time_t *epoch_out)
{
    if (!cJSON_IsObject(item) || !epoch_out) {
        return false;
    }

    const cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(item, "lu");
    if (cJSON_IsNumber(timestamp) && timestamp->valuedouble > 0.0) {
        *epoch_out = (time_t)timestamp->valuedouble;
        return true;
    }

    timestamp = cJSON_GetObjectItemCaseSensitive(item, "last_changed");
    if (!cJSON_IsString(timestamp)) {
        timestamp = cJSON_GetObjectItemCaseSensitive(item, "last_updated");
    }
    return cJSON_IsString(timestamp) && timestamp->valuestring &&
           ha_datetime_parse_epoch(timestamp->valuestring, epoch_out) ==
               ESP_OK;
}

static void sensor_history_insert_record(time_t epoch, const char *state)
{
    if (epoch <= 0 || !state || !state[0]) {
        return;
    }

    size_t position = 0;
    while (position < s_sensor_history.record_count &&
           s_sensor_history.records[position].epoch > epoch) {
        position++;
    }
    /* Keep different state changes within the same second. Only skip an
     * adjacent duplicate, which can occur when live data overlaps the
     * history response. */
    if (position < s_sensor_history.record_count &&
        s_sensor_history.records[position].epoch == epoch &&
        strcmp(s_sensor_history.records[position].state, state) == 0) {
        return;
    }
    if (position >= SENSOR_HISTORY_LIST_MAX_RECORDS) {
        return;
    }

    const size_t new_count =
        s_sensor_history.record_count < SENSOR_HISTORY_LIST_MAX_RECORDS
            ? s_sensor_history.record_count + 1
            : SENSOR_HISTORY_LIST_MAX_RECORDS;
    for (size_t i = new_count - 1; i > position; i--) {
        s_sensor_history.records[i] = s_sensor_history.records[i - 1];
    }
    s_sensor_history.records[position].epoch = epoch;
    snprintf(s_sensor_history.records[position].state,
             sizeof(s_sensor_history.records[position].state),
             "%s",
             state);
    s_sensor_history.record_count = new_count;
}

static bool sensor_history_parse_list_result(const cJSON *result)
{
    if (!cJSON_IsObject(result)) {
        return false;
    }

    const cJSON *states = cJSON_GetObjectItemCaseSensitive(
        result,
        s_sensor_history.entity_id);
    /* HA omits the entity key when the requested period has no history. */
    if (!states && !result->child) {
        return true;
    }
    if (!cJSON_IsArray(states)) {
        return false;
    }

    /* Merge the response with live changes received while loading. */
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, states) {
        time_t epoch = 0;
        if (!sensor_history_parse_epoch(item, &epoch)) {
            continue;
        }
        const cJSON *state = cJSON_GetObjectItemCaseSensitive(item, "s");
        if (!cJSON_IsString(state)) {
            state = cJSON_GetObjectItemCaseSensitive(item, "state");
        }
        if (!cJSON_IsString(state) || !state->valuestring) {
            continue;
        }
        sensor_history_insert_record(epoch, state->valuestring);
    }
    return true;
}

static bool sensor_history_parse_item(const cJSON *item,
                                      sensor_history_point_t *point_out)
{
    if (!cJSON_IsObject(item) || !point_out) {
        return false;
    }

    const cJSON *state = cJSON_GetObjectItemCaseSensitive(item, "s");
    if (!cJSON_IsString(state)) {
        state = cJSON_GetObjectItemCaseSensitive(item, "state");
    }
    double value = 0.0;
    if (!cJSON_IsString(state) || !state->valuestring ||
        !sensor_parse_numeric_state(state->valuestring, &value)) {
        return false;
    }

    time_t epoch = 0;
    const cJSON *last_updated = cJSON_GetObjectItemCaseSensitive(item, "lu");
    if (cJSON_IsNumber(last_updated) && last_updated->valuedouble > 0.0) {
        epoch = (time_t)last_updated->valuedouble;
    }
    else {
        last_updated = cJSON_GetObjectItemCaseSensitive(item, "last_updated");
        if (!cJSON_IsString(last_updated) || !last_updated->valuestring ||
            ha_datetime_parse_epoch(last_updated->valuestring, &epoch) !=
                ESP_OK) {
            return false;
        }
    }

    memset(point_out, 0, sizeof(*point_out));
    point_out->epoch = epoch;
    point_out->value = value;
    return true;
}

static bool sensor_history_parse_result(const cJSON *result)
{
    if (!cJSON_IsObject(result)) {
        return false;
    }

    const cJSON *states = cJSON_GetObjectItemCaseSensitive(
        result,
        s_sensor_history.entity_id);
    /* HA omits the entity key when the requested period has no history. */
    if (!states && !result->child) {
        return true;
    }
    if (!cJSON_IsArray(states)) {
        return false;
    }

    size_t valid_count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, states) {
        sensor_history_point_t point = {0};
        if (sensor_history_parse_item(item, &point)) {
            valid_count++;
        }
    }

    const size_t live_count = s_sensor_history.point_count;
    if (valid_count > SIZE_MAX - live_count) {
        return false;
    }

    const size_t required_count = valid_count + live_count;
    if (required_count == 0) {
        free(s_sensor_history.points);
        s_sensor_history.points = NULL;
        s_sensor_history.point_count = 0;
        s_sensor_history.point_capacity = 0;
        return true;
    }
    if (required_count > SIZE_MAX / sizeof(*s_sensor_history.points)) {
        return false;
    }

    sensor_history_point_t *merged = calloc(required_count,
                                             sizeof(*merged));
    if (!merged) {
        return false;
    }

    size_t merged_count = 0;
    cJSON_ArrayForEach(item, states) {
        sensor_history_point_t point = {0};
        if (!sensor_history_parse_item(item, &point)) {
            continue;
        }
        merged[merged_count++] = point;
    }

    for (size_t i = 0; i < live_count; i++) {
        const sensor_history_point_t point = s_sensor_history.points[i];
        if (merged_count > 0 &&
            merged[merged_count - 1].epoch == point.epoch &&
            fabs(merged[merged_count - 1].value - point.value) < 0.000001) {
            continue;
        }
        merged[merged_count++] = point;
    }

    free(s_sensor_history.points);
    s_sensor_history.points = merged;
    s_sensor_history.point_count = merged_count;
    s_sensor_history.point_capacity = required_count;
    sensor_history_prune(s_sensor_history.end_time);
    return true;
}

static bool sensor_history_reserve(size_t required_capacity)
{
    if (required_capacity <= s_sensor_history.point_capacity) {
        return true;
    }
    if (required_capacity > SIZE_MAX / sizeof(*s_sensor_history.points)) {
        return false;
    }

    size_t new_capacity = s_sensor_history.point_capacity
                              ? s_sensor_history.point_capacity
                              : 16;
    while (new_capacity < required_capacity) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = required_capacity;
            break;
        }
        new_capacity *= 2;
    }

    sensor_history_point_t *points = realloc(
        s_sensor_history.points,
        new_capacity * sizeof(*points));
    if (!points) {
        return false;
    }
    s_sensor_history.points = points;
    s_sensor_history.point_capacity = new_capacity;
    return true;
}

static void sensor_history_prune(time_t now_epoch)
{
    if (now_epoch <= 0) {
        return;
    }

    s_sensor_history.end_time = now_epoch;
    s_sensor_history.start_time =
        now_epoch - SENSOR_HISTORY_HOURS * 60 * 60;

    size_t first_visible = 0;
    while (first_visible < s_sensor_history.point_count &&
           s_sensor_history.points[first_visible].epoch <
               s_sensor_history.start_time) {
        first_visible++;
    }
    if (first_visible == 0) {
        return;
    }

    const size_t remaining = s_sensor_history.point_count - first_visible;
    if (remaining > 0) {
        memmove(s_sensor_history.points,
                s_sensor_history.points + first_visible,
                remaining * sizeof(*s_sensor_history.points));
    }
    s_sensor_history.point_count = remaining;
}

static void sensor_history_observe_state(
    const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot) {
        return;
    }

    if (!s_sensor_history.observed_state_initialized) {
        snprintf(s_sensor_history.observed_state,
                 sizeof(s_sensor_history.observed_state),
                 "%s",
                 snapshot->state);
        s_sensor_history.observed_state_initialized = true;
        return;
    }
    if (strcmp(s_sensor_history.observed_state, snapshot->state) == 0) {
        return;
    }

    snprintf(s_sensor_history.observed_state,
             sizeof(s_sensor_history.observed_state),
             "%s",
             snapshot->state);

    if (s_sensor_history.is_list_history) {
        time_t state_epoch = 0;
        const char *changed_at = snapshot->last_changed[0]
                                     ? snapshot->last_changed
                                     : snapshot->last_updated;
        if (ha_datetime_parse_epoch(changed_at, &state_epoch) == ESP_OK) {
            sensor_history_insert_record(state_epoch, snapshot->state);
        }
        return;
    }

    double value = 0.0;
    const time_t now_epoch = time(NULL);
    if (s_sensor_history.failed || now_epoch <= 0 ||
        !sensor_parse_numeric_state(snapshot->state, &value)) {
        return;
    }
    if (!sensor_history_reserve(s_sensor_history.point_count + 1)) {
        ESP_LOGW(TAG,
                 "Failed to append live sensor history for %s",
                 s_sensor_history.entity_id);
        return;
    }

    sensor_history_point_t *point =
        &s_sensor_history.points[s_sensor_history.point_count++];
    point->epoch = now_epoch;
    point->value = value;
    sensor_history_prune(now_epoch);
}

static void sensor_history_reset(void)
{
    free(s_sensor_history.points);
    memset(&s_sensor_history, 0, sizeof(s_sensor_history));
    s_sensor_history.request_id = -1;
}

static void sensor_history_screen_delete_cb(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_sensor_history.screen) {
        /* History belongs to this More Info session. Releasing it here makes
         * the next open fetch a fresh history response. Resetting request_id
         * also makes a late response from the closed screen harmless. */
        sensor_history_reset();
    }
}

static void sensor_history_prepare(
    const entity_more_info_context_t *context)
{
    const bool entity_changed =
        strcmp(s_sensor_history.entity_id, context->snapshot.entity_id) != 0;

    const bool is_list_history =
        !sensor_has_chart_history(&context->snapshot);
    const bool history_mode_changed =
        s_sensor_history.is_list_history != is_list_history;

    if (entity_changed || history_mode_changed) {
        /* Keep the existing screen delete callback when switching modes. */
        lv_obj_t *screen = s_sensor_history.screen;
        sensor_history_reset();
        s_sensor_history.screen = screen;
        snprintf(s_sensor_history.entity_id,
                 sizeof(s_sensor_history.entity_id),
                 "%s",
                 context->snapshot.entity_id);
    }

    if (s_sensor_history.screen != context->screen) {
        s_sensor_history.screen = context->screen;
        if (s_sensor_history.failed) {
            /* A failed request may be retried when the user opens a new More
             * Info screen, but not during refreshes of the current screen. */
            s_sensor_history.failed = false;
        }
        lv_obj_add_event_cb(context->screen,
                            sensor_history_screen_delete_cb,
                            LV_EVENT_DELETE,
                            NULL);
    }

    s_sensor_history.is_list_history = is_list_history;
    snprintf(s_sensor_history.device_class,
             sizeof(s_sensor_history.device_class),
             "%s",
             context->snapshot.device_class);
    snprintf(s_sensor_history.unit,
             sizeof(s_sensor_history.unit),
             "%s",
             context->snapshot.unit_of_measurement);
    sensor_history_observe_state(&context->snapshot);

    const time_t now_epoch = time(NULL);
    if (s_sensor_history.loaded && now_epoch > 0 &&
        !s_sensor_history.is_list_history) {
        sensor_history_prune(now_epoch);
    }

    if (s_sensor_history.loading || s_sensor_history.loaded ||
        s_sensor_history.failed) {
        return;
    }

    const time_t end_time = now_epoch;
    if (end_time <= 0) {
        s_sensor_history.failed = true;
        return;
    }

    s_sensor_history.start_time =
        s_sensor_history.is_list_history
            ? end_time - SENSOR_HISTORY_LIST_DAYS * 24 * 60 * 60
            : end_time - SENSOR_HISTORY_HOURS * 60 * 60;
    s_sensor_history.end_time = end_time;
    s_sensor_history.loading = true;

    int request_id = 0;
    const esp_err_t err = app_ha_get_entity_history(
        s_sensor_history.entity_id,
        s_sensor_history.start_time,
        s_sensor_history.end_time,
        sensor_history_result_cb,
        &s_sensor_history,
        &request_id);
    if (err != ESP_OK) {
        s_sensor_history.loading = false;
        s_sensor_history.failed = true;
        ESP_LOGW(TAG,
                 "Failed to request sensor history for %s: %s",
                 s_sensor_history.entity_id,
                 esp_err_to_name(err));
        return;
    }
    s_sensor_history.request_id = request_id;
}

static void sensor_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx)
{
    (void)error;
    sensor_history_cache_t *history = user_ctx;
    if (history != &s_sensor_history || !lvgl_port_lock(0)) {
        return;
    }

    if (request_id != history->request_id) {
        lvgl_port_unlock();
        return;
    }

    history->loading = false;
    history->loaded =
        err == ESP_OK && (history->is_list_history
                              ? sensor_history_parse_list_result(result)
                              : sensor_history_parse_result(result));
    history->failed = !history->loaded;
    if (history->failed) {
        ESP_LOGW(TAG,
                 "Failed to load sensor history for %s: %s",
                 history->entity_id,
                 err == ESP_OK ? "invalid response" : esp_err_to_name(err));
    }

    ha_entity_store_t *store = app_get_ha_entity_store();
    if (store) {
        more_info_screen_refresh_entity(store, history->entity_id);
    }
    lvgl_port_unlock();
}

static void sensor_history_render(lv_obj_t *parent)
{
    if (s_sensor_history.is_list_history) {
        const char *message = NULL;
        if (s_sensor_history.loading && s_sensor_history.record_count == 0) {
            message = i18n_get(I18N_HISTORY_LOADING);
        }
        else if (s_sensor_history.failed &&
                 s_sensor_history.record_count == 0) {
            message = i18n_get(I18N_HISTORY_UNAVAILABLE);
        }
        else if (s_sensor_history.record_count == 0) {
            message = i18n_get(I18N_HISTORY_EMPTY);
        }

        const bool format_timestamp =
            strcmp(s_sensor_history.device_class, "timestamp") == 0 ||
            strcmp(s_sensor_history.device_class, "uptime") == 0;
        ha_history_list_item_t items[SENSOR_HISTORY_LIST_MAX_RECORDS] = {0};
        char formatted_states[SENSOR_HISTORY_LIST_MAX_RECORDS]
                             [ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
        for (size_t i = 0; i < s_sensor_history.record_count; i++) {
            items[i].epoch = s_sensor_history.records[i].epoch;
            items[i].text = entity_state_translate(s_sensor_history.records[i].state);
            if (format_timestamp &&
                sensor_format_timestamp_state(items[i].text,
                                              formatted_states[i],
                                              sizeof(formatted_states[i]))) {
                items[i].text = formatted_states[i];
            }
        }

        const ha_history_list_config_t config = {
            .title = i18n_get(I18N_HISTORY_TITLE),
            .items = items,
            .item_count = s_sensor_history.record_count,
            .message = message,
        };
        if (!ha_history_list_create(parent, &config)) {
            ESP_LOGW(TAG, "Failed to create sensor history list");
        }
        return;
    }

    const char *message = NULL;
    if (s_sensor_history.loading) {
        message = i18n_get(I18N_HISTORY_LOADING);
    }
    else if (s_sensor_history.failed) {
        message = i18n_get(I18N_HISTORY_UNAVAILABLE);
    }
    else if (s_sensor_history.point_count == 0) {
        message = i18n_get(I18N_HISTORY_EMPTY);
    }

    const ha_history_chart_config_t config = {
        .title = i18n_get(I18N_HISTORY_TITLE),
        .unit = s_sensor_history.unit,
        .points = s_sensor_history.points,
        .point_count = s_sensor_history.point_count,
        .start_time = s_sensor_history.start_time,
        .end_time = s_sensor_history.end_time,
        .message = message,
    };
    if (!ha_history_chart_create(parent, &config)) {
        ESP_LOGW(TAG, "Failed to create sensor history chart");
    }
}

static const char *sensor_get_icon(const ha_entity_snapshot_t *snapshot)
{
    if (snapshot && snapshot->device_class[0]) {
        double value = 0.0;
        if (sensor_parse_numeric_state(snapshot->state, &value)) {
            const char *range_icon = NULL;
            for (size_t i = 0;
                 i < sizeof(s_sensor_range_icons) / sizeof(s_sensor_range_icons[0]);
                 i++) {
                const sensor_range_icon_mapping_t *mapping =
                    &s_sensor_range_icons[i];
                if (strcmp(snapshot->device_class, mapping->device_class) == 0 &&
                    value >= mapping->threshold) {
                    range_icon = mapping->icon;
                }
            }
            if (range_icon) {
                return range_icon;
            }
        }

        for (size_t i = 0;
             i < sizeof(s_sensor_icons) / sizeof(s_sensor_icons[0]);
             i++) {
            const sensor_icon_mapping_t *mapping = &s_sensor_icons[i];
            if (strcmp(snapshot->device_class, mapping->device_class) == 0) {
                return mapping->icon;
            }
        }
    }

    return MDI_SYMBOL_EYE;
}

/* Match HA's default sensor icon colors in its dark theme. */
static uint32_t sensor_icon_color(const ha_entity_snapshot_t *snapshot)
{
    if (entity_handler_snapshot_is_service_unavailable(snapshot)) {
        return 0x6F6F6F;
    }

    if (strcmp(snapshot->device_class, "battery") == 0) {
        double level = 0.0;
        if (sensor_parse_numeric_state(snapshot->state, &level)) {
            if (level >= 70.0) {
                return 0x4CAF50;
            }
            if (level >= 30.0) {
                return 0xFF9800;
            }
            return 0xF44336;
        }
    }

    return 0x44739E;
}

static const char *sensor_display_numeric_state(const ha_entity_snapshot_t *snapshot,
                                                char *buffer, size_t buffer_size)
{
    double value = 0;
    if (!sensor_parse_numeric_state(snapshot->state, &value)) {
        return snapshot->state;
    }
    if (snapshot->has_display_precision) {
        int length = snprintf(buffer, buffer_size, "%.*f",
                              snapshot->display_precision, value);
        return length >= 0 && (size_t)length < buffer_size
                   ? buffer : snapshot->state;
    }

    /* Preserve the digits sent by HA when its state uses scientific notation. */
    const char *exponent = strchr(snapshot->state, 'e');
    if (!exponent) {
        exponent = strchr(snapshot->state, 'E');
    }
    if (!exponent) {
        return snapshot->state;
    }
    char *end = NULL;
    long power = strtol(exponent + 1, &end, 10);
    if (!end || *end != '\0' || power < -1000 || power > 1000) {
        return snapshot->state;
    }
    const char *mantissa = snapshot->state;
    const bool negative = *mantissa == '-';
    if (*mantissa == '-' || *mantissa == '+') {
        mantissa++;
    }
    char digits[HA_ENTITY_STORE_STATE_MAX];
    size_t digit_count = 0;
    int decimal_at = -1;
    for (const char *p = mantissa; p < exponent; p++) {
        if (*p == '.') {
            decimal_at = (int)digit_count;
        } else if (digit_count + 1 < sizeof(digits)) {
            digits[digit_count++] = *p;
        } else {
            return snapshot->state;
        }
    }
    if (decimal_at < 0) {
        decimal_at = (int)digit_count;
    }
    long point = decimal_at + power;
    size_t length = (negative ? 1 : 0) +
                    (point <= 0 ? 2 + (size_t)-point + digit_count :
                     point >= (long)digit_count ? (size_t)point : digit_count + 1);
    if (length >= buffer_size) {
        return snapshot->state;
    }
    char *out = buffer;
    if (negative) {
        *out++ = '-';
    }
    if (point <= 0) {
        *out++ = '0';
        *out++ = '.';
        for (long i = 0; i < -point; i++) {
            *out++ = '0';
        }
        memcpy(out, digits, digit_count);
        out += digit_count;
    } else if (point >= (long)digit_count) {
        memcpy(out, digits, digit_count);
        out += digit_count;
        for (long i = (long)digit_count; i < point; i++) {
            *out++ = '0';
        }
    } else {
        memcpy(out, digits, (size_t)point);
        out += point;
        *out++ = '.';
        memcpy(out, digits + point, digit_count - (size_t)point);
        out += digit_count - (size_t)point;
    }
    *out = '\0';
    return buffer;
}

static void sensor_format_state(const ha_entity_snapshot_t *snapshot,
                                char *output,
                                size_t output_size)
{
    if (!snapshot || !output || output_size == 0) {
        return;
    }

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(output, output_size, snapshot);
        return;
    }

    char numeric_state[HA_ENTITY_STORE_STATE_MAX + 64];
    const char *display_state = sensor_display_numeric_state(
        snapshot, numeric_state, sizeof(numeric_state));
    if (snapshot->unit_of_measurement[0]) {
        const char *separator =
            strcmp(snapshot->unit_of_measurement, "%") == 0 ||
            strcmp(snapshot->unit_of_measurement, "°") == 0
                ? ""
                : " ";
        snprintf(output,
                 output_size,
                 "%s%s%s",
                 display_state,
                 separator,
                 snapshot->unit_of_measurement);
        return;
    }

    if (display_state == snapshot->state) {
        entity_handler_copy_snapshot_state(output, output_size, snapshot);
    } else {
        snprintf(output, output_size, "%s", display_state);
    }
}

static bool sensor_has_timestamp_state(const ha_entity_snapshot_t *snapshot)
{
    return snapshot &&
           (strcmp(snapshot->device_class, "timestamp") == 0 ||
            strcmp(snapshot->device_class, "uptime") == 0);
}

static bool sensor_format_timestamp_state(const char *state,
                                          char *output,
                                          size_t output_size)
{
    time_t state_epoch = 0;
    if (!state || !output || output_size == 0 ||
        ha_datetime_parse_epoch(state, &state_epoch) != ESP_OK ||
        state_epoch < 0) {
        return false;
    }

    char local_iso[40] = {0};
    if (sys_time_format_iso8601_time_by_utc_ms(
            (uint64_t)state_epoch * 1000,
            local_iso,
            sizeof(local_iso)) != 0) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    if (sscanf(local_iso, "%4d-%2d-%2dT%2d:%2d",
               &year, &month, &day, &hour, &minute) != 5 ||
        month < 1 || month > 12) {
        return false;
    }

    return i18n_format_datetime(output, output_size, (unsigned)year,
                                 (unsigned)month, (unsigned)day,
                                 (unsigned)hour, (unsigned)minute);
}

static time_t sensor_format_more_info_state(const ha_entity_snapshot_t *snapshot,
                                            char *output,
                                            size_t output_size)
{
    if (!snapshot || !output || output_size == 0) {
        return 0;
    }

    if (!entity_handler_snapshot_is_unavailable(snapshot) &&
        strcmp(snapshot->device_class, "uptime") == 0) {
        time_t state_epoch = 0;
        if (ha_datetime_parse_epoch(snapshot->state, &state_epoch) == ESP_OK &&
            state_epoch > 0 &&
            ha_datetime_format_elapsed_epoch(state_epoch,
                                             time(NULL),
                                             output,
                                             output_size) == ESP_OK) {
            return state_epoch;
        }
    }

    if (!entity_handler_snapshot_is_unavailable(snapshot) &&
        sensor_has_timestamp_state(snapshot) &&
        sensor_format_timestamp_state(snapshot->state, output, output_size)) {
        return 0;
    }

    sensor_format_state(snapshot, output, output_size);
    return 0;
}

static bool sensor_handle_action(const entity_action_context_t *context)
{
    if (!context) {
        return false;
    }

    return entity_handler_handle_more_info_action(context);
}

static void sensor_more_info_close_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) {
        context->close(context->close_user_data);
    }
}

static lv_obj_t *sensor_more_info_build_page(
    const entity_more_info_context_t *context)
{
    lv_obj_t *screen = context->screen;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x010203), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(screen, 0, 0);

    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 20, 0);
    lv_obj_set_style_pad_ver(header, 12, 0);
    lv_obj_set_style_pad_gap(header, 10, 0);

    lv_obj_t *close_button = lv_image_create(header);
    lv_img_set_src(close_button, &close_50);
    lv_obj_add_flag(close_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_button,
                        sensor_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    ui_i18n_label_set_text(title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_height(title, lv_font_get_line_height(&ha_font_misan_medium_24));

    lv_obj_t *close_hitbox = lv_obj_create(context->overlay);
    lv_obj_remove_style_all(close_hitbox);
    lv_obj_remove_flag(close_hitbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(close_hitbox, 50, 50);
    lv_obj_align(close_hitbox, LV_ALIGN_TOP_LEFT, 20, 12);
    lv_obj_add_event_cb(close_hitbox,
                        sensor_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));

    lv_obj_t *main_ctn = lv_obj_create(screen);
    lv_obj_remove_style_all(main_ctn);
    lv_obj_set_scroll_dir(main_ctn, LV_DIR_VER);
    lv_obj_set_size(main_ctn, LV_PCT(100), 0);
    lv_obj_set_flex_grow(main_ctn, 1);
    lv_obj_set_layout(main_ctn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_ctn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_ctn,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(main_ctn, 8, 0);
    return main_ctn;
}

static esp_err_t sensor_render_tile(ha_entity_store_t *store,
                                   const ha_entity_snapshot_t *snapshot,
                                   entity_display_model_t *model)
{
    (void)store;
    if (!snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Entity-store updates append live states to the active history session.
     * A unit change is handled by rebuilding the history in prepare(). */
    if (s_sensor_history.is_list_history ==
            !sensor_has_chart_history(snapshot) &&
        s_sensor_history.entity_id[0] &&
        strcmp(s_sensor_history.entity_id, snapshot->entity_id) == 0) {
        sensor_history_observe_state(snapshot);
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = sensor_get_icon(snapshot);
    model->background_color = lv_color_hex(0x1E2228);
    model->icon_color = lv_color_hex(sensor_icon_color(snapshot));
    model->icon_circle_background_color = lv_color_hex(0x242d36);
    model->title_color = lv_color_hex(0xFFFFFF);
    model->state_color = lv_color_hex(0x666666);

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon_circle_background_color = lv_color_hex(0x2c2c2c);
    } else {
        time_t state_epoch = 0;
        if (sensor_has_timestamp_state(snapshot) &&
            ha_datetime_parse_epoch(snapshot->state, &state_epoch) == ESP_OK) {
            const bool uptime =
                strcmp(snapshot->device_class, "uptime") == 0;
            if (uptime) {
                ha_datetime_format_elapsed_epoch(state_epoch,
                                                 time(NULL),
                                                 model->state_text,
                                                 sizeof(model->state_text));
            }
            else {
                ha_datetime_format_relative_now(snapshot->state,
                                                model->state_text,
                                                sizeof(model->state_text));
            }
            model->state_text_type = uptime
                                         ? ENTITY_STATE_TEXT_ELAPSED_TIME
                                         : ENTITY_STATE_TEXT_RELATIVE_TIME;
            model->state_time = state_epoch;
        } else {
            sensor_format_state(snapshot,
                                model->state_text,
                                sizeof(model->state_text));
        }
    }
    return ESP_OK;
}

static esp_err_t sensor_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->close) {
        return ESP_ERR_INVALID_ARG;
    }

    lv_obj_t *main_ctn = sensor_more_info_build_page(context);

    lv_obj_t *content = lv_obj_create(main_ctn);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, bsp_lcd_is_landscape() ? SCREEN_DEFAULT_CONTENT_WIDTH_LANDSCAPE : SCREEN_DEFAULT_CONTENT_WIDTH_PORTAIT);
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(content, 12, 0);
    lv_obj_set_style_pad_ver(content, 8, 0);
    lv_obj_set_style_pad_gap(content, 10, 0);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x1E2228), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(content, 12, 0);


    lv_obj_t *tile_box = lv_obj_create(content);
    lv_obj_remove_style_all(tile_box);
    lv_obj_set_size(tile_box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(tile_box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tile_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile_box,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tile_box, 0, 0);
    lv_obj_set_style_pad_gap(tile_box, 12, 0);


    lv_obj_t *tile_icon = lv_label_create(tile_box);
    ui_i18n_label_set_text(tile_icon, sensor_get_icon(&context->snapshot));
    lv_obj_set_style_text_font(tile_icon, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(
        tile_icon,
        lv_color_hex(sensor_icon_color(&context->snapshot)),
        0);

    lv_obj_t *tile_content_ctx = lv_obj_create(tile_box);
    lv_obj_remove_style_all(tile_content_ctx);
    lv_obj_set_flex_grow(tile_content_ctx, 1);
    lv_obj_set_height(tile_content_ctx, LV_SIZE_CONTENT);
    lv_obj_set_layout(tile_content_ctx, LV_LAYOUT_FLEX);
    if (bsp_lcd_is_landscape()) {
        lv_obj_set_flex_flow(tile_content_ctx, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_gap(tile_content_ctx,
                                 SENSOR_TITLE_STATE_GAP_LANDSCAPE, 0);
        lv_obj_set_flex_align(tile_content_ctx,
                              LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_set_flex_flow(tile_content_ctx, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tile_content_ctx,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER);
    }
    lv_obj_set_style_pad_all(tile_content_ctx, 0, 0);

    lv_obj_t *left_col = tile_content_ctx;
    if (bsp_lcd_is_landscape()) {
        left_col = lv_obj_create(tile_content_ctx);
        lv_obj_remove_style_all(left_col);
        lv_obj_set_layout(left_col, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(left_col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(left_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(left_col, 0, 0);
        lv_obj_set_size(left_col, 0, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(left_col, 1);
    }

    lv_obj_t *tile_content_title = lv_label_create(left_col);
    lv_obj_set_style_text_font(tile_content_title, &ha_font_misan_medium_24, 0);
    lv_obj_set_size(tile_content_title,
                    bsp_lcd_is_landscape() ? 0 : LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_medium_24));
    lv_label_set_long_mode(tile_content_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(tile_content_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_height(tile_content_title, lv_font_get_line_height(&ha_font_misan_medium_24));
    ui_i18n_label_set_text(tile_content_title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);

    lv_obj_t *tile_content_cur_state = lv_label_create(tile_content_ctx);
    lv_obj_set_style_text_font(tile_content_cur_state, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(tile_content_cur_state, lv_color_white(), 0);
    char cur_state_text[ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
    const time_t relative_state_epoch =
        sensor_format_more_info_state(&context->snapshot,
                                      cur_state_text,
                                      sizeof(cur_state_text));
    ui_i18n_label_set_text(tile_content_cur_state, cur_state_text);
    if (relative_state_epoch > 0) {
        ha_relative_time_label_bind_elapsed(tile_content_cur_state,
                                            relative_state_epoch);
    }
    if (bsp_lcd_is_landscape()) {
        lv_obj_set_size(tile_content_cur_state,
                        0,
                        lv_font_get_line_height(&ha_font_misan_regular_16));
        lv_label_set_long_mode(tile_content_cur_state, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(tile_content_cur_state, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_obj_t *tile_content_state = lv_label_create(left_col);
    char state_text[ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
    time_t last_updated_epoch = 0;
    if (ha_datetime_parse_epoch(context->snapshot.last_updated,
                                &last_updated_epoch) == ESP_OK) {
        ha_datetime_format_relative_epoch(last_updated_epoch,
                                          time(NULL),
                                          state_text,
                                          sizeof(state_text));
    }
    if (!state_text[0]) {
        i18n_copy(state_text, sizeof(state_text), i18n_get(I18N_STATE_UNKNOWN));
    }
    ui_i18n_label_set_text(tile_content_state, state_text);
    if (last_updated_epoch > 0) {
        ha_relative_time_label_bind(tile_content_state,
                                    last_updated_epoch);
    }

    lv_obj_set_style_text_font(tile_content_state, &ha_font_misan_regular_16, 0);
    lv_obj_set_size(tile_content_state,
                    bsp_lcd_is_landscape() ? 0 : LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_regular_16));
    lv_label_set_long_mode(tile_content_state, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(tile_content_state, lv_color_hex(0x666666), 0);

    if (bsp_lcd_is_landscape()) {
        lv_obj_update_layout(content);
        lv_point_t state_size;
        lv_text_get_size(&state_size, cur_state_text,
                         &ha_font_misan_regular_16,
                         lv_obj_get_style_text_letter_space(
                             tile_content_cur_state, LV_PART_MAIN),
                         0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        const int32_t max_state_width =
            lv_obj_get_content_width(tile_content_ctx) -
            SENSOR_TITLE_MIN_WIDTH_LANDSCAPE -
            SENSOR_TITLE_STATE_GAP_LANDSCAPE;
        lv_obj_set_width(tile_content_cur_state,
                         LV_MIN(state_size.x, LV_MAX(max_state_width, 0)));
        lv_obj_update_layout(content);
        const int32_t left_width = lv_obj_get_content_width(left_col);
        lv_obj_set_width(tile_content_title, left_width);
        lv_obj_set_width(tile_content_state, left_width);
    }

    sensor_history_prepare(context);
    sensor_history_render(main_ctn);

    return ESP_OK;
}

const entity_handler_t sensor_entity_handler = {
    .domain = "sensor",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_EYE,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_NOTHING },
    },
    .handle_action = sensor_handle_action,
    .render_tile = sensor_render_tile,
    .render_more_info = sensor_render_more_info,
};
