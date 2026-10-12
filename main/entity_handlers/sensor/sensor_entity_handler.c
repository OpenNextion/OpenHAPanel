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
#include "screen/components/ha_relative_time_label.h"
#include "screen/components/more_info_screen.h"

#define SCREEN_DEFAULT_CONTENT_WIDTH 280
#define SENSOR_HISTORY_HOURS 12

static const char *TAG = "sensor_entity_handler";

typedef ha_history_chart_point_t sensor_history_point_t;

typedef struct {
    lv_obj_t *screen;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    char unit[HA_ENTITY_STORE_UNIT_MAX];
    time_t start_time;
    time_t end_time;
    int request_id;
    bool loading;
    bool loaded;
    bool failed;
    bool observed_state_initialized;
    char observed_state[HA_ENTITY_STORE_STATE_MAX];
    size_t point_count;
    size_t point_capacity;
    sensor_history_point_t *points;
} sensor_history_cache_t;

static sensor_history_cache_t s_sensor_history;

typedef struct {
    const char *device_class;
    const char *icon;
} sensor_icon_mapping_t;

/* Keep this mapping aligned with the frontend sensor icon rules. */
static const sensor_icon_mapping_t s_sensor_icons[] = {
    {"apparent_power", MDI_SYMBOL_FLASH},
    {"aqi", MDI_SYMBOL_AIR_FILTER},
    {"atmospheric_pressure", MDI_SYMBOL_THERMOMETER_LINES},
    {"carbon_dioxide", MDI_SYMBOL_MOLECULE_CO2},
    {"carbon_monoxide", MDI_SYMBOL_MOLECULE_CO},
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
    {"reactive_power", MDI_SYMBOL_FLASH},
    {"signal_strength", MDI_SYMBOL_WIFI},
    {"sound_pressure", MDI_SYMBOL_EAR_HEARING},
    {"speed", MDI_SYMBOL_SPEEDOMETER},
    {"sulfur_dioxide", MDI_SYMBOL_MOLECULE},
    {"temperature", MDI_SYMBOL_THERMOMETER},
    {"timestamp", MDI_SYMBOL_CLOCK},
    {"uptime", MDI_SYMBOL_CLOCK_START},
    {"volatile_organic_compounds", MDI_SYMBOL_MOLECULE},
    {"volatile_organic_compounds_parts", MDI_SYMBOL_MOLECULE},
    {"voltage", MDI_SYMBOL_SINE_WAVE},
    {"volume", MDI_SYMBOL_CAR_COOLANT_LEVEL},
    {"volume_storage", MDI_SYMBOL_STORAGE_TANK},
    {"water", MDI_SYMBOL_WATER},
    {"weight", MDI_SYMBOL_WEIGHT},
    {"wind_speed", MDI_SYMBOL_WEATHER_WINDY},
};

static bool sensor_parse_numeric_state(const char *state, double *value_out);
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

    if (entity_changed) {
        sensor_history_reset();
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

    snprintf(s_sensor_history.unit,
             sizeof(s_sensor_history.unit),
             "%s",
             context->snapshot.unit_of_measurement);
    sensor_history_observe_state(&context->snapshot);

    const time_t now_epoch = time(NULL);
    if (s_sensor_history.loaded && now_epoch > 0) {
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
        end_time - SENSOR_HISTORY_HOURS * 60 * 60;
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
    history->loaded = err == ESP_OK && sensor_history_parse_result(result);
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
    const char *message = NULL;
    if (s_sensor_history.loading) {
        message = "Loading history...";
    }
    else if (s_sensor_history.failed) {
        message = "History unavailable";
    }
    else if (s_sensor_history.point_count == 0) {
        message = "No history";
    }

    const ha_history_chart_config_t config = {
        .title = "History",
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

static bool sensor_device_class_is(const ha_entity_snapshot_t *snapshot,
                                   const char *device_class)
{
    return snapshot && device_class &&
           strcmp(snapshot->device_class, device_class) == 0;
}

static const char *sensor_get_icon(const ha_entity_snapshot_t *snapshot)
{
    if (snapshot && snapshot->device_class[0]) {
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

static bool sensor_device_class_has_history(
    const ha_entity_snapshot_t *snapshot)
{
    return sensor_device_class_is(snapshot, "temperature") ||
           sensor_device_class_is(snapshot, "humidity");
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

    if (sensor_device_class_has_history(snapshot) &&
        snapshot->unit_of_measurement[0]) {
        snprintf(output,
                 output_size,
                 "%s%s",
                 snapshot->state,
                 snapshot->unit_of_measurement);
        return;
    }

    entity_handler_copy_snapshot_state(output, output_size, snapshot);
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
    lv_label_set_text(title,
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

    /* Keep the retained history current while More Info is closed. Tile
     * refreshes are driven by entity-store changes, so no HA history request
     * is needed for new temperature or humidity samples. */
    if (sensor_device_class_has_history(snapshot) &&
        s_sensor_history.entity_id[0] &&
        strcmp(s_sensor_history.entity_id, snapshot->entity_id) == 0) {
        sensor_history_observe_state(snapshot);
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = sensor_get_icon(snapshot);
    model->background_color = lv_color_hex(0x1E2228);
    model->icon_color = lv_color_hex(0x44739e);
    model->icon_circle_background_color = lv_color_hex(0x242d36);
    model->title_color = lv_color_hex(0xFFFFFF);
    model->state_color = lv_color_hex(0x666666);
    model->has_ripple_color = true;
    model->ripple_color = lv_color_hex(0xD9D9D9);

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon_color = lv_color_hex(0x6f6f6f);
        model->icon_circle_background_color = lv_color_hex(0x2c2c2c);
    } else {
        sensor_format_state(snapshot,
                                model->state_text,
                                sizeof(model->state_text));
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
    lv_obj_set_width(content, SCREEN_DEFAULT_CONTENT_WIDTH);
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
    lv_label_set_text(tile_icon, sensor_get_icon(&context->snapshot));
    lv_obj_set_style_text_font(tile_icon, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(tile_icon, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *tile_content_ctx = lv_obj_create(tile_box);
    lv_obj_remove_style_all(tile_content_ctx);
    lv_obj_set_flex_grow(tile_content_ctx, 1);
    lv_obj_set_height(tile_content_ctx, LV_SIZE_CONTENT);
    lv_obj_set_layout(tile_content_ctx, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tile_content_ctx, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile_content_ctx,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tile_content_ctx, 0, 0);

    lv_obj_t *tile_content_title = lv_label_create(tile_content_ctx);
    lv_obj_set_style_text_font(tile_content_title, &ha_font_misan_medium_24, 0);
    lv_obj_set_size(tile_content_title, LV_PCT(100), lv_font_get_line_height(&ha_font_misan_medium_24));
    lv_label_set_long_mode(tile_content_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(tile_content_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_height(tile_content_title, lv_font_get_line_height(&ha_font_misan_medium_24));
    lv_label_set_text(tile_content_title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);

    lv_obj_t *tile_content_cur_state = lv_label_create(tile_content_ctx);
    lv_obj_set_style_text_font(tile_content_cur_state, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(tile_content_cur_state, lv_color_white(), 0);
    char cur_state_text[ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
    sensor_format_state(&context->snapshot,
                    cur_state_text,
                    sizeof(cur_state_text));
    lv_label_set_text(tile_content_cur_state, cur_state_text);

    lv_obj_t *tile_content_state = lv_label_create(tile_content_ctx);
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
        snprintf(state_text, sizeof(state_text), "Unknown");
    }
    lv_label_set_text(tile_content_state, state_text);
    if (last_updated_epoch > 0) {
        ha_relative_time_label_bind(tile_content_state,
                                    last_updated_epoch);
    }

    lv_obj_set_style_text_font(tile_content_state, &ha_font_misan_regular_16, 0);
    lv_obj_set_size(tile_content_state, LV_PCT(100), lv_font_get_line_height(&ha_font_misan_regular_16));
    lv_label_set_long_mode(tile_content_state, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(tile_content_state, lv_color_hex(0x666666), 0);

    if (!sensor_device_class_has_history(&context->snapshot)) {
        return ESP_OK;
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
