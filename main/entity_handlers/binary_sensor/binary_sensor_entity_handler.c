#include "ui_i18n.h"
#include "ha_ui_font.h"
#include "i18n.h"
#include "binary_sensor_entity_handler.h"
#include "entity_handlers/entity_state_translation.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "bsp_lcd.h"
#include "entity_handlers/ha_datetime_format.h"
#include "entity_handlers/light/components/light_detail_theme.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "screen/components/ha_history_list.h"
#include "screen/components/ha_relative_time_label.h"
#include "screen/components/more_info_screen.h"

#define SCREEN_DEFAULT_CONTENT_WIDTH_PORTRAIT 280
#define SCREEN_DEFAULT_CONTENT_WIDTH_LANDSCAPE 440
#define BINARY_SENSOR_TITLE_MIN_WIDTH_LANDSCAPE 120
#define BINARY_SENSOR_TITLE_STATE_GAP_LANDSCAPE 10
#define BINARY_SENSOR_HISTORY_DAYS 3
#define BINARY_SENSOR_HISTORY_MAX_RECORDS 14

#define BINARY_SENSOR_COLOR_ALERT       0xF44336
#define BINARY_SENSOR_COLOR_ACTIVE      0xFFC107
#define BINARY_SENSOR_COLOR_INACTIVE    0x9E9E9E
#define BINARY_SENSOR_COLOR_STATE_ICON  0x44739E
#define BINARY_SENSOR_COLOR_UNAVAILABLE 0x6F6F6F
#define BINARY_SENSOR_TILE_ICON_BG      0x1C1C1C

static const char *TAG = "binary_sensor_entity_handler";

typedef struct {
    time_t epoch;
    char state[HA_ENTITY_STORE_STATE_MAX];
} binary_sensor_history_record_t;

typedef struct {
    lv_obj_t *screen;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    char entity_state[HA_ENTITY_STORE_STATE_MAX];
    char device_class[HA_ENTITY_STORE_DEVICE_CLASS_MAX];
    int request_id;
    bool loading;
    bool loaded;
    bool failed;
    size_t record_count;
    binary_sensor_history_record_t records[BINARY_SENSOR_HISTORY_MAX_RECORDS];
} binary_sensor_history_cache_t;

static binary_sensor_history_cache_t s_binary_sensor_history;


typedef struct {
    const char *device_class;
    const char *off_icon;
    const char *on_icon;
} binary_sensor_icon_t;

/* Keep this mapping aligned with the frontend binary_sensor icon rules. */
static const binary_sensor_icon_t s_binary_sensor_icons[] = {
    {"battery", MDI_SYMBOL_BATTERY, MDI_SYMBOL_BATTERY_OUTLINE},
    {"battery_charging", MDI_SYMBOL_BATTERY, MDI_SYMBOL_BATTERY_CHARGING},
    {"carbon_monoxide", MDI_SYMBOL_SMOKE_DETECTOR,
     MDI_SYMBOL_SMOKE_DETECTOR_ALERT},
    {"cold", MDI_SYMBOL_THERMOMETER, MDI_SYMBOL_SNOWFLAKE},
    {"connectivity", MDI_SYMBOL_CLOSE_NETWORK_OUTLINE,
     MDI_SYMBOL_CHECK_NETWORK_OUTLINE},
    {"door", MDI_SYMBOL_DOOR_CLOSED, MDI_SYMBOL_DOOR_OPEN},
    {"garage_door", MDI_SYMBOL_GARAGE, MDI_SYMBOL_GARAGE_OPEN},
    {"gas", MDI_SYMBOL_CHECK_CIRCLE, MDI_SYMBOL_ALERT_CIRCLE},
    {"glass_break", MDI_SYMBOL_GLASS_FRAGILE, MDI_SYMBOL_GLASS_FRAGILE},
    {"heat", MDI_SYMBOL_THERMOMETER, MDI_SYMBOL_FIRE},
    {"light", MDI_SYMBOL_BRIGHTNESS_5, MDI_SYMBOL_BRIGHTNESS_7},
    {"lock", MDI_SYMBOL_LOCK, MDI_SYMBOL_LOCK_OPEN},
    {"moisture", MDI_SYMBOL_WATER_OFF, MDI_SYMBOL_WATER},
    {"motion", MDI_SYMBOL_MOTION_SENSOR_OFF, MDI_SYMBOL_MOTION_SENSOR},
    {"moving", MDI_SYMBOL_OCTAGON, MDI_SYMBOL_ARROW_RIGHT},
    {"occupancy", MDI_SYMBOL_HOME_OUTLINE, MDI_SYMBOL_HOME},
    {"opening", MDI_SYMBOL_SQUARE, MDI_SYMBOL_SQUARE_OUTLINE},
    {"plug", MDI_SYMBOL_POWER_PLUG_OFF, MDI_SYMBOL_POWER_PLUG},
    {"power", MDI_SYMBOL_POWER_PLUG_OFF, MDI_SYMBOL_POWER_PLUG},
    {"presence", MDI_SYMBOL_HOME_OUTLINE, MDI_SYMBOL_HOME},
    {"problem", MDI_SYMBOL_CHECK_CIRCLE, MDI_SYMBOL_ALERT_CIRCLE},
    {"running", MDI_SYMBOL_STOP, MDI_SYMBOL_PLAY},
    {"safety", MDI_SYMBOL_CHECK_CIRCLE, MDI_SYMBOL_ALERT_CIRCLE},
    {"smoke", MDI_SYMBOL_SMOKE_DETECTOR_VARIANT,
     MDI_SYMBOL_SMOKE_DETECTOR_VARIANT_ALERT},
    {"sound", MDI_SYMBOL_MUSIC_NOTE_OFF, MDI_SYMBOL_MUSIC_NOTE},
    {"tamper", MDI_SYMBOL_CHECK_CIRCLE, MDI_SYMBOL_ALERT_CIRCLE},
    {"update", MDI_SYMBOL_PACKAGE, MDI_SYMBOL_PACKAGE_UP},
    {"vibration", MDI_SYMBOL_CROP_PORTRAIT, MDI_SYMBOL_VIBRATE},
    {"window", MDI_SYMBOL_WINDOW_CLOSED, MDI_SYMBOL_WINDOW_OPEN},
};

static bool binary_sensor_is_on(const ha_entity_snapshot_t *snapshot)
{
    return snapshot &&
           snapshot->sync_status != HA_ENTITY_SYNC_REMOVED &&
           strcmp(snapshot->state, "on") == 0;
}

static bool binary_sensor_uses_alert_color(const char *device_class)
{
    if (!device_class || !device_class[0]) {
        return false;
    }

    static const char *const alert_device_classes[] = {
        "battery",
        "carbon_monoxide",
        "gas",
        "glass_break",
        "heat",
        "lock",
        "moisture",
        "problem",
        "safety",
        "smoke",
        "sound",
        "tamper",
    };

    for (size_t i = 0;
         i < sizeof(alert_device_classes) / sizeof(alert_device_classes[0]);
         i++) {
        if (strcmp(device_class, alert_device_classes[i]) == 0) {
            return true;
        }
    }
    return false;
}

static uint32_t binary_sensor_active_color(const char *device_class)
{
    return binary_sensor_uses_alert_color(device_class)
               ? BINARY_SENSOR_COLOR_ALERT
               : BINARY_SENSOR_COLOR_ACTIVE;
}

static uint32_t binary_sensor_icon_color(
    const ha_entity_snapshot_t *snapshot,
    bool more_info)
{
    if (entity_handler_snapshot_is_service_unavailable(snapshot)) {
        return BINARY_SENSOR_COLOR_UNAVAILABLE;
    }
    if (binary_sensor_is_on(snapshot)) {
        return binary_sensor_active_color(snapshot->device_class);
    }
    return more_info ? BINARY_SENSOR_COLOR_STATE_ICON
                     : BINARY_SENSOR_COLOR_INACTIVE;
}

static lv_color_t binary_sensor_tile_icon_background_color(
    uint32_t icon_color)
{
    /* ha-tile-icon renders the state color over its background at 20%. */
    return lv_color_mix(lv_color_hex(icon_color),
                        lv_color_hex(BINARY_SENSOR_TILE_ICON_BG),
                        LV_OPA_20);
}

static const char *binary_sensor_get_icon(
    const ha_entity_snapshot_t *snapshot)
{
    const bool is_on = binary_sensor_is_on(snapshot);
    const char *off_icon = MDI_SYMBOL_RADIOBOX_BLANK;
    const char *on_icon = MDI_SYMBOL_CHECKBOX_MARKED_CIRCLE;

    if (snapshot && snapshot->device_class[0]) {
        for (size_t i = 0;
             i < sizeof(s_binary_sensor_icons) /
                     sizeof(s_binary_sensor_icons[0]);
             i++) {
            const binary_sensor_icon_t *mapping = &s_binary_sensor_icons[i];
            if (strcmp(snapshot->device_class, mapping->device_class) == 0) {
                off_icon = mapping->off_icon;
                on_icon = mapping->on_icon;
                break;
            }
        }
    }

    return is_on ? on_icon : off_icon;
}

static const char *binary_sensor_get_state_text(const char *state,
                                                const char *device_class)
{
    return entity_state_translate_binary(state, device_class);
}

static void binary_sensor_format_state_value(const char *state,
                                             const char *device_class,
                                             char *output,
                                             size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }

    snprintf(output,
             output_size,
             "%s",
             binary_sensor_get_state_text(state, device_class));
}

static void binary_sensor_format_state(const ha_entity_snapshot_t *snapshot,
                                       char *output,
                                       size_t output_size)
{
    if (!snapshot || !output || output_size == 0) {
        return;
    }

    if (snapshot->sync_status == HA_ENTITY_SYNC_REMOVED) {
        i18n_copy(output, output_size, i18n_get(I18N_STATE_UNAVAILABLE));
        return;
    }

    binary_sensor_format_state_value(snapshot->state,
                                     snapshot->device_class,
                                     output,
                                     output_size);
}

static lv_obj_t *binary_sensor_create_more_info_state_label(
    lv_obj_t *parent,
    const ha_entity_snapshot_t *snapshot)
{
    lv_obj_t *label = lv_label_create(parent);
    char state_text[ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
    binary_sensor_format_state(snapshot, state_text, sizeof(state_text));
    ui_i18n_label_set_text(label, state_text);
    lv_obj_set_style_text_font(label, ha_ui_font_regular(16), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(light_detail_theme_color(0xFFFFFF, LIGHT_DETAIL_LIGHT_TEXT_PRIMARY)), 0);
    return label;
}

static void binary_sensor_history_prepare(
    const entity_more_info_context_t *context);
static void binary_sensor_history_request(const char *entity_id);
static void binary_sensor_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx);
static bool binary_sensor_history_parse_result(const cJSON *result);
static bool binary_sensor_history_parse_epoch(const cJSON *item,
                                              time_t *epoch_out);
static void binary_sensor_history_insert(time_t epoch, const char *state);
static void binary_sensor_history_observe_state(
    const ha_entity_snapshot_t *snapshot);
static void binary_sensor_history_screen_delete_cb(lv_event_t *event);
static void binary_sensor_history_reset(void);
static void binary_sensor_history_render(lv_obj_t *parent);

static void binary_sensor_history_prepare(
    const entity_more_info_context_t *context)
{
    const char *entity_id = context->snapshot.entity_id;
    const bool entity_changed =
        strcmp(s_binary_sensor_history.entity_id, entity_id) != 0;

    if (entity_changed) {
        binary_sensor_history_reset();
        snprintf(s_binary_sensor_history.entity_id,
                 sizeof(s_binary_sensor_history.entity_id),
                 "%s",
                 entity_id);
        snprintf(s_binary_sensor_history.entity_state,
                 sizeof(s_binary_sensor_history.entity_state),
                 "%s",
                 context->snapshot.state);
    }

    if (s_binary_sensor_history.screen != context->screen) {
        s_binary_sensor_history.screen = context->screen;
        if (s_binary_sensor_history.failed) {
            s_binary_sensor_history.failed = false;
        }
        lv_obj_add_event_cb(context->screen,
                            binary_sensor_history_screen_delete_cb,
                            LV_EVENT_DELETE,
                            NULL);
    }

    snprintf(s_binary_sensor_history.device_class,
             sizeof(s_binary_sensor_history.device_class),
             "%s",
             context->snapshot.device_class);

    binary_sensor_history_observe_state(&context->snapshot);

    binary_sensor_history_request(entity_id);
}

static void binary_sensor_history_request(const char *entity_id)
{
    if (!entity_id || !entity_id[0] || s_binary_sensor_history.loading ||
        s_binary_sensor_history.loaded || s_binary_sensor_history.failed) {
        return;
    }

    const time_t end_time = time(NULL);
    if (end_time <= 0) {
        s_binary_sensor_history.failed = true;
        return;
    }

    s_binary_sensor_history.loading = true;
    int request_id = 0;
    const esp_err_t err = app_ha_get_entity_history(
        entity_id,
        end_time - BINARY_SENSOR_HISTORY_DAYS * 24 * 60 * 60,
        end_time,
        binary_sensor_history_result_cb,
        &s_binary_sensor_history,
        &request_id);
    if (err != ESP_OK) {
        s_binary_sensor_history.loading = false;
        s_binary_sensor_history.failed = true;
        ESP_LOGW(TAG,
                 "Failed to request history for %s: %s",
                 entity_id,
                 esp_err_to_name(err));
        return;
    }
    s_binary_sensor_history.request_id = request_id;
}

static void binary_sensor_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx)
{
    (void)error;
    binary_sensor_history_cache_t *history = user_ctx;
    if (history != &s_binary_sensor_history || !lvgl_port_lock(0)) {
        return;
    }

    if (request_id != history->request_id) {
        lvgl_port_unlock();
        return;
    }

    history->loading = false;
    history->loaded = err == ESP_OK && binary_sensor_history_parse_result(result);
    history->failed = !history->loaded;

    if (history->failed) {
        if (err == ESP_OK) {
            ESP_LOGW(TAG,
                     "Invalid history response for %s",
                     history->entity_id);
        }
        else {
            ESP_LOGW(TAG,
                     "Failed to load history for %s: %s",
                     history->entity_id,
                     esp_err_to_name(err));
        }
    }

    ha_entity_store_t *store = app_get_ha_entity_store();
    if (store) {
        more_info_screen_refresh_entity(store, history->entity_id);
    }
    lvgl_port_unlock();
}

static bool binary_sensor_history_parse_result(const cJSON *result)
{
    if (!cJSON_IsObject(result)) {
        return false;
    }

    const cJSON *states = cJSON_GetObjectItemCaseSensitive(
        result,
        s_binary_sensor_history.entity_id);
    if (!cJSON_IsArray(states)) {
        return false;
    }

    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, states) {
        time_t epoch = 0;
        const cJSON *state = cJSON_GetObjectItemCaseSensitive(item, "s");
        if (!cJSON_IsString(state)) {
            state = cJSON_GetObjectItemCaseSensitive(item, "state");
        }
        if (cJSON_IsString(state) && state->valuestring &&
            binary_sensor_history_parse_epoch(item, &epoch)) {
            binary_sensor_history_insert(epoch, state->valuestring);
        }
    }
    return true;
}

static bool binary_sensor_history_parse_epoch(const cJSON *item,
                                              time_t *epoch_out)
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

static void binary_sensor_history_insert(time_t epoch, const char *state)
{
    if (epoch <= 0 || !state || !state[0]) {
        return;
    }

    size_t position = 0;
    while (position < s_binary_sensor_history.record_count &&
           s_binary_sensor_history.records[position].epoch > epoch) {
        position++;
    }
    if (position < s_binary_sensor_history.record_count &&
        s_binary_sensor_history.records[position].epoch == epoch &&
        strcmp(s_binary_sensor_history.records[position].state, state) == 0) {
        return;
    }
    if (position >= BINARY_SENSOR_HISTORY_MAX_RECORDS) {
        return;
    }

    const size_t new_count =
        s_binary_sensor_history.record_count < BINARY_SENSOR_HISTORY_MAX_RECORDS
            ? s_binary_sensor_history.record_count + 1
            : BINARY_SENSOR_HISTORY_MAX_RECORDS;
    for (size_t i = new_count - 1; i > position; i--) {
        s_binary_sensor_history.records[i] = s_binary_sensor_history.records[i - 1];
    }
    s_binary_sensor_history.records[position].epoch = epoch;
    snprintf(s_binary_sensor_history.records[position].state,
             sizeof(s_binary_sensor_history.records[position].state),
             "%s",
             state);
    s_binary_sensor_history.record_count = new_count;
}

static void binary_sensor_history_observe_state(
    const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot || !s_binary_sensor_history.entity_id[0] ||
        strcmp(s_binary_sensor_history.entity_id, snapshot->entity_id) != 0 ||
        strcmp(s_binary_sensor_history.entity_state, snapshot->state) == 0) {
        return;
    }

    snprintf(s_binary_sensor_history.entity_state,
             sizeof(s_binary_sensor_history.entity_state),
             "%s",
             snapshot->state);

    time_t state_epoch = 0;
    const char *changed_at = snapshot->last_changed[0]
                                 ? snapshot->last_changed
                                 : snapshot->last_updated;
    if (ha_datetime_parse_epoch(changed_at, &state_epoch) == ESP_OK) {
        binary_sensor_history_insert(state_epoch, snapshot->state);
    }
}

static void binary_sensor_history_screen_delete_cb(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_binary_sensor_history.screen) {
        s_binary_sensor_history.screen = NULL;
    }
}

static void binary_sensor_history_reset(void)
{
    memset(&s_binary_sensor_history, 0, sizeof(s_binary_sensor_history));
    s_binary_sensor_history.request_id = -1;
}

static void binary_sensor_history_render(lv_obj_t *parent)
{
    const char *message = NULL;
    if (s_binary_sensor_history.loading && s_binary_sensor_history.record_count == 0) {
        message = i18n_get(I18N_HISTORY_LOADING);
    }
    else if (s_binary_sensor_history.failed &&
             s_binary_sensor_history.record_count == 0) {
        message = i18n_get(I18N_HISTORY_UNAVAILABLE);
    }
    else if (s_binary_sensor_history.record_count == 0) {
        message = i18n_get(I18N_HISTORY_EMPTY);
    }

    ha_history_list_item_t items[BINARY_SENSOR_HISTORY_MAX_RECORDS] = {0};
    for (size_t i = 0; i < s_binary_sensor_history.record_count; i++) {
        items[i].epoch = s_binary_sensor_history.records[i].epoch;
        items[i].text = binary_sensor_get_state_text(
            s_binary_sensor_history.records[i].state,
            s_binary_sensor_history.device_class);
    }

    const ha_history_list_config_t config = {
        .title = i18n_get(I18N_HISTORY_TITLE),
        .items = items,
        .item_count = s_binary_sensor_history.record_count,
        .message = message,
    };
    if (!ha_history_list_create(parent, &config)) {
        ESP_LOGW(TAG, "Failed to create binary sensor history list");
    }
}

static bool binary_sensor_handle_action(const entity_action_context_t *context)
{
    if (!context) {
        return false;
    }

    return entity_handler_handle_more_info_action(context);
}

static lv_obj_t *binary_sensor_more_info_build_page(
    const entity_more_info_context_t *context)
{
    lv_obj_t *main_ctn = lv_obj_create(context->body);
    lv_obj_remove_style_all(main_ctn);
    lv_obj_set_scroll_dir(main_ctn, LV_DIR_VER);
    lv_obj_set_size(main_ctn, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(main_ctn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_ctn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_ctn,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(main_ctn, 8, 0);
    return main_ctn;
}

static esp_err_t binary_sensor_render_tile(ha_entity_store_t *store,
                                   const ha_entity_snapshot_t *snapshot,
                                   entity_display_model_t *model)
{
    (void)store;
    if (!snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_binary_sensor_history.entity_id[0] &&
        strcmp(s_binary_sensor_history.entity_id, snapshot->entity_id) == 0) {
        binary_sensor_history_observe_state(snapshot);
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = binary_sensor_get_icon(snapshot);

    binary_sensor_format_state(snapshot,
                               model->state_text,
                               sizeof(model->state_text));
    model->background_color = lv_color_hex(0x1E2228);
    const uint32_t icon_color = binary_sensor_icon_color(snapshot, false);
    model->icon_color = lv_color_hex(icon_color);
    model->icon_circle_background_color =
        binary_sensor_tile_icon_background_color(icon_color);
    model->title_color = lv_color_hex(0xFFFFFF);
    model->state_color = lv_color_hex(0x666666);
    return ESP_OK;
}

static esp_err_t binary_sensor_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->body) {
        return ESP_ERR_INVALID_ARG;
    }

    lv_obj_t *main_ctn = binary_sensor_more_info_build_page(context);
    const bool landscape = bsp_lcd_is_landscape();

    lv_obj_t *content = lv_obj_create(main_ctn);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content,
                     landscape
                         ? SCREEN_DEFAULT_CONTENT_WIDTH_LANDSCAPE
                         : SCREEN_DEFAULT_CONTENT_WIDTH_PORTRAIT);
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content,
                         landscape ? LV_FLEX_FLOW_ROW
                                   : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          landscape ? LV_FLEX_ALIGN_CENTER
                                    : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(content, 12, 0);
    lv_obj_set_style_pad_ver(content, 8, 0);
    lv_obj_set_style_pad_gap(content, 10, 0);
    lv_obj_set_style_bg_color(content, lv_color_hex(light_detail_theme_color(0x1E2228, LIGHT_DETAIL_LIGHT_SURFACE)), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(content, 12, 0);

    lv_obj_t *tile_box = lv_obj_create(content);
    lv_obj_remove_style_all(tile_box);
    lv_obj_set_size(tile_box,
                    landscape ? 0 : LV_PCT(100),
                    LV_SIZE_CONTENT);
    if (landscape) {
        lv_obj_set_flex_grow(tile_box, 1);
    }
    lv_obj_set_layout(tile_box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tile_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile_box,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tile_box, 0, 0);
    lv_obj_set_style_pad_gap(tile_box, 12, 0);


    lv_obj_t *tile_icon = lv_label_create(tile_box);
    ui_i18n_label_set_text(tile_icon, binary_sensor_get_icon(&context->snapshot));
    lv_obj_set_style_text_font(tile_icon, &ha_font_mdi_26, 0);
    const uint32_t icon_color =
        binary_sensor_icon_color(&context->snapshot, true);
    lv_obj_set_style_text_color(tile_icon, lv_color_hex(icon_color), 0);


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
    lv_obj_set_style_text_font(tile_content_title, ha_ui_font_medium(24), 0);
    lv_obj_set_size(tile_content_title,
                    landscape ? 0 : LV_PCT(100),
                    lv_font_get_line_height(ha_ui_font_medium(24)));
    lv_label_set_long_mode(tile_content_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(tile_content_title, lv_color_hex(light_detail_theme_color(0xFFFFFF, LIGHT_DETAIL_LIGHT_TEXT_PRIMARY)), 0);
    // 展示实体的friendly_name
    ui_i18n_label_set_text(tile_content_title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);

    if (!landscape) {
        binary_sensor_create_more_info_state_label(tile_content_ctx,
                                                   &context->snapshot);
    }

    lv_obj_t *tile_content_state = lv_label_create(tile_content_ctx);
    // 展示 binary_sensor 最近一次状态变化的相对时间
    char state_text[HA_ENTITY_STORE_STATE_MAX] = {0};
    const char *changed_at = context->snapshot.last_changed[0]
                                 ? context->snapshot.last_changed
                                 : context->snapshot.last_updated;
    if (changed_at[0] &&
        ha_datetime_format_relative_now(changed_at,
                                        state_text,
                                        sizeof(state_text)) != ESP_OK) {
        state_text[0] = '\0';
    }
    ui_i18n_label_set_text(tile_content_state, state_text);
    if (state_text[0]) {
        time_t state_time = 0;
        if (ha_datetime_parse_epoch(changed_at,
                                    &state_time) == ESP_OK) {
            ha_relative_time_label_bind(tile_content_state, state_time);
        }
    }
    else {
        lv_obj_add_flag(tile_content_state, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_text_font(tile_content_state, ha_ui_font_regular(16), 0);
    lv_obj_set_size(tile_content_state, LV_PCT(100), lv_font_get_line_height(ha_ui_font_regular(16)));
    lv_label_set_long_mode(tile_content_state, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(tile_content_state, lv_color_hex(0x666666), 0);

    if (landscape) {
        lv_obj_t *state_box = lv_obj_create(content);
        lv_obj_remove_style_all(state_box);
        lv_obj_remove_flag(state_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(state_box, 0, 48);

        lv_obj_t *current_state =
            binary_sensor_create_more_info_state_label(state_box,
                                                       &context->snapshot);
        lv_obj_update_layout(content);
        lv_point_t state_size;
        lv_text_get_size(&state_size, lv_label_get_text(current_state),
                         ha_ui_font_regular(16),
                         lv_obj_get_style_text_letter_space(
                             current_state, LV_PART_MAIN),
                         0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        const int32_t max_state_width =
            lv_obj_get_content_width(content) -
            lv_obj_get_width(tile_icon) - 12 -
            BINARY_SENSOR_TITLE_MIN_WIDTH_LANDSCAPE -
            BINARY_SENSOR_TITLE_STATE_GAP_LANDSCAPE;
        const int32_t state_width =
            LV_MIN(state_size.x, LV_MAX(max_state_width, 0));
        lv_obj_set_width(state_box, state_width);
        lv_obj_set_size(current_state, state_width,
                        lv_font_get_line_height(ha_ui_font_regular(16)));
        lv_label_set_long_mode(current_state, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(current_state, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(current_state);

        lv_obj_update_layout(content);
        lv_obj_set_width(tile_content_title,
                         lv_obj_get_content_width(tile_content_ctx));
    }

    binary_sensor_history_prepare(context);
    binary_sensor_history_render(main_ctn);

    return ESP_OK;
}

const entity_handler_t binary_sensor_entity_handler = {
    .domain = "binary_sensor",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_RADIOBOX_BLANK,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_NOTHING },
    },
    .handle_action = binary_sensor_handle_action,
    .render_tile = binary_sensor_render_tile,
    .render_more_info = binary_sensor_render_more_info,
};
