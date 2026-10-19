#include "event_entity_handler.h"

#include <string.h>
#include <time.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "screen/components/ha_history_list.h"
#include "screen/components/ha_relative_time_label.h"
#include "screen/components/more_info_screen.h"

#define EVENT_TILE_BG           0x1E2228
#define EVENT_TILE_ICON         0x44739E
#define EVENT_TILE_TITLE        0xFFFFFF
#define EVENT_TILE_STATE        0x666666

#define SCREEN_DEFAULT_CONTENT_WIDTH 280
#define EVENT_HISTORY_MAX_RECORDS 14
#define EVENT_HISTORY_DAYS 3

static const char *TAG = "event_entity_handler";
static void event_history_prepare(
    const entity_more_info_context_t *context);
static void event_history_request(const char *entity_id);
static void event_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx);
static bool event_history_parse_result(const cJSON *result);
static bool event_history_parse_epoch(const cJSON *item,
                                      time_t *epoch_out);
static void event_history_insert(time_t epoch,
                                  const char *state,
                                  const char *event_type);
static const char *event_history_format_event_type(
    const char *event_type,
    const char *device_class);
static void event_history_observe_state(
    const ha_entity_snapshot_t *snapshot);
static void event_history_screen_delete_cb(lv_event_t *event);
static void event_history_reset(void);
static void event_history_render(lv_obj_t *parent);

typedef struct {
    const char *device_class;
    const char *icon;
} event_icon_mapping_t;

static const event_icon_mapping_t s_event_icons[] = {
    {"doorbell", MDI_SYMBOL_DOORBELL},
    {"button",   MDI_SYMBOL_GESTURE_TAP_BUTTON},
    {"motion",   MDI_SYMBOL_MOTION_SENSOR},
};

typedef struct {
    time_t epoch;
    char state[HA_ENTITY_STORE_STATE_MAX];
    char event_type[64];
} event_history_record_t;

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
    event_history_record_t records[EVENT_HISTORY_MAX_RECORDS];
} event_history_cache_t;

static event_history_cache_t s_event_history;

static void event_history_reset(void)
{
    memset(&s_event_history, 0, sizeof(s_event_history));
    s_event_history.request_id = -1;
}

static void event_history_prepare(
    const entity_more_info_context_t *context)
{
    const char *entity_id = context->snapshot.entity_id;
    const bool entity_changed =
        strcmp(s_event_history.entity_id, entity_id) != 0;

    if (entity_changed) {
        event_history_reset();
        snprintf(s_event_history.entity_id,
                 sizeof(s_event_history.entity_id),
                 "%s",
                 entity_id);
    }

    if (s_event_history.screen != context->screen) {
        s_event_history.screen = context->screen;
        if (s_event_history.failed) {
            s_event_history.failed = false;
        }
        lv_obj_add_event_cb(context->screen,
                            event_history_screen_delete_cb,
                            LV_EVENT_DELETE,
                            NULL);
    }

    snprintf(s_event_history.device_class,
             sizeof(s_event_history.device_class),
             "%s",
             context->snapshot.device_class);

    event_history_observe_state(&context->snapshot);

    event_history_request(entity_id);
}

static void event_history_request(const char *entity_id)
{
    if (!entity_id || !entity_id[0] || s_event_history.loading ||
        s_event_history.loaded || s_event_history.failed) {
        return;
    }

    const time_t end_time = time(NULL);
    if (end_time <= 0) {
        s_event_history.failed = true;
        return;
    }

    s_event_history.loading = true;
    int request_id = 0;
    const esp_err_t err = app_ha_get_entity_history(
        entity_id,
        end_time - EVENT_HISTORY_DAYS * 24 * 60 * 60,
        end_time,
        event_history_result_cb,
        &s_event_history,
        &request_id);
    if (err != ESP_OK) {
        s_event_history.loading = false;
        s_event_history.failed = true;
        ESP_LOGW(TAG,
                 "Failed to request history for %s: %s",
                 entity_id,
                 esp_err_to_name(err));
        return;
    }
    s_event_history.request_id = request_id;
}

static void event_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx)
{
    (void)error;
    event_history_cache_t *history = user_ctx;
    if (history != &s_event_history || !lvgl_port_lock(0)) {
        return;
    }

    if (request_id != history->request_id) {
        lvgl_port_unlock();
        return;
    }

    history->loading = false;
    history->loaded =
        err == ESP_OK && event_history_parse_result(result);
    history->failed = !history->loaded;

    if (history->loaded) {
        ESP_LOGI(TAG,
                 "History loaded for %s: %zu records",
                 history->entity_id,
                 history->record_count);
    }

    if (history->failed) {
        if (err == ESP_OK) {
            ESP_LOGW(TAG,
                     "Invalid history response for %s",
                     history->entity_id);
        } else {
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

static bool event_history_parse_result(const cJSON *result)
{
    if (!cJSON_IsObject(result)) {
        return false;
    }

    const cJSON *states = cJSON_GetObjectItemCaseSensitive(
        result, s_event_history.entity_id);
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
        const char *event_type = "";
        const cJSON *attrs =
            cJSON_GetObjectItemCaseSensitive(item, "a");
        if (!cJSON_IsObject(attrs)) {
            attrs = cJSON_GetObjectItemCaseSensitive(item, "attributes");
        }
        if (cJSON_IsObject(attrs)) {
            const cJSON *et = cJSON_GetObjectItemCaseSensitive(
                attrs, "event_type");
            if (cJSON_IsString(et) && et->valuestring &&
                et->valuestring[0]) {
                event_type = et->valuestring;
            } else {
                const cJSON *ets =
                    cJSON_GetObjectItemCaseSensitive(attrs, "event_types");
                if (cJSON_IsArray(ets)) {
                    const cJSON *first = cJSON_GetArrayItem(ets, 0);
                    if (cJSON_IsString(first) && first->valuestring) {
                        event_type = first->valuestring;
                    }
                } else if (cJSON_IsString(ets) && ets->valuestring) {
                    event_type = ets->valuestring;
                }
            }
        }

        if (cJSON_IsString(state) && state->valuestring &&
            event_history_parse_epoch(item, &epoch)) {
            event_history_insert(epoch, state->valuestring, event_type);
        }
    }
    return true;
}

static bool event_history_parse_epoch(const cJSON *item,
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

static void event_history_insert(time_t epoch,
                                  const char *state,
                                  const char *event_type)
{
    if (epoch <= 0 || !state || !state[0]) {
        return;
    }

    size_t position = 0;
    while (position < s_event_history.record_count &&
           s_event_history.records[position].epoch > epoch) {
        position++;
    }
    if (position < s_event_history.record_count &&
        s_event_history.records[position].epoch == epoch) {
        snprintf(s_event_history.records[position].state,
                 sizeof(s_event_history.records[position].state),
                 "%s",
                 state);
        if (event_type && event_type[0]) {
            snprintf(s_event_history.records[position].event_type,
                     sizeof(s_event_history.records[position].event_type),
                     "%s",
                     event_type);
        }
        return;
    }
    if (position >= EVENT_HISTORY_MAX_RECORDS) {
        return;
    }

    const size_t new_count =
        s_event_history.record_count < EVENT_HISTORY_MAX_RECORDS
            ? s_event_history.record_count + 1
            : EVENT_HISTORY_MAX_RECORDS;
    for (size_t i = new_count - 1; i > position; i--) {
        s_event_history.records[i] =
            s_event_history.records[i - 1];
    }
    s_event_history.records[position].epoch = epoch;
    snprintf(s_event_history.records[position].state,
             sizeof(s_event_history.records[position].state),
             "%s",
             state);
    snprintf(s_event_history.records[position].event_type,
             sizeof(s_event_history.records[position].event_type),
             "%s",
             event_type ? event_type : "");
    s_event_history.record_count = new_count;
}

static void event_history_observe_state(
    const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot || !s_event_history.entity_id[0] ||
        strcmp(s_event_history.entity_id, snapshot->entity_id) != 0 ||
        strcmp(s_event_history.entity_state, snapshot->state) == 0) {
        return;
    }

    snprintf(s_event_history.entity_state,
             sizeof(s_event_history.entity_state),
             "%s",
             snapshot->state);

    time_t state_epoch = 0;
    const char *changed_at = snapshot->last_changed[0]
                                 ? snapshot->last_changed
                                 : snapshot->last_updated;
    if (ha_datetime_parse_epoch(changed_at, &state_epoch) == ESP_OK) {
        event_history_insert(state_epoch, snapshot->state, "");
    }
}

static void event_history_screen_delete_cb(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_event_history.screen) {
        s_event_history.screen = NULL;
    }
}

static void event_history_render(lv_obj_t *parent)
{
    const char *message = NULL;
    if (s_event_history.loading &&
        s_event_history.record_count == 0) {
        message = "Loading history...";
    } else if (s_event_history.failed &&
               s_event_history.record_count == 0) {
        message = "History unavailable";
    } else if (s_event_history.record_count == 0) {
        message = "No history";
    }

    ha_history_list_item_t items[EVENT_HISTORY_MAX_RECORDS] = {0};
    for (size_t i = 0; i < s_event_history.record_count; i++) {
        items[i].epoch = s_event_history.records[i].epoch;
        items[i].text = strcmp(s_event_history.records[i].state,
                                "unavailable") == 0
                            ? "Unavailable"
                            : "Event_triggered";
    }

    const ha_history_list_config_t config = {
        .title = "History",
        .items = items,
        .item_count = s_event_history.record_count,
        .message = message,
    };
    if (!ha_history_list_create(parent, &config)) {
        ESP_LOGW(TAG, "Failed to create event history list");
    }
}

static const char *event_history_format_event_type(
    const char *event_type,
    const char *device_class)
{
    if (event_type && event_type[0]) {
        static char formatted[64];
        size_t pos = 0;
        bool new_word = true;
        for (const char *p = event_type; *p && pos < sizeof(formatted) - 1;
             p++) {
            if (*p == '_') {
                formatted[pos++] = ' ';
                new_word = true;
            } else if (new_word) {
                formatted[pos++] =
                    (char)((*p >= 'a' && *p <= 'z') ? *p - 'a' + 'A' : *p);
                new_word = false;
            } else {
                formatted[pos++] = *p;
            }
        }
        formatted[pos] = '\0';
        return formatted;
    }

    if (device_class) {
        if (strcmp(device_class, "doorbell") == 0) return "Ring";
        if (strcmp(device_class, "button") == 0) return "Press";
        if (strcmp(device_class, "motion") == 0) return "Detected";
    }
    return "Event";
}

static const char *event_get_icon(const ha_entity_snapshot_t *snapshot)
{
    if (snapshot && snapshot->device_class[0]) {
        for (size_t i = 0;
             i < sizeof(s_event_icons) / sizeof(s_event_icons[0]);
             i++) {
            const event_icon_mapping_t *mapping = &s_event_icons[i];
            if (strcmp(snapshot->device_class, mapping->device_class) == 0) {
                return mapping->icon;
            }
        }
    }

    return MDI_SYMBOL_DOORBELL;
}

static bool event_handle_action(const entity_action_context_t *context)
{
    if (entity_handler_handle_more_info_action(context)) {
        return true;
    }

    if (!context || !context->entity_id) {
        return false;
    }

    if (context->trigger == ENTITY_ACTION_TRIGGER_TAP &&
        context->open_more_info) {
        context->open_more_info(context->entity_id, context->user_data);
        return true;
    }

    return true;
}

static esp_err_t event_render_tile(ha_entity_store_t *store,
                                    const ha_entity_snapshot_t *snapshot,
                                    entity_display_model_t *model)
{
    (void)store;
    if (!snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG,
             "tile icon probe: entity=%s state='%s' name='%s' "
             "device_class='%s'",
             snapshot->entity_id,
             snapshot->state,
             snapshot->friendly_name,
             snapshot->device_class);

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = event_get_icon(snapshot);
    model->background_color = lv_color_hex(EVENT_TILE_BG);
    model->icon_color = lv_color_hex(EVENT_TILE_ICON);
    // model->icon_circle_background_color = lv_color_hex(0x242d36);
    model->title_color = lv_color_hex(EVENT_TILE_TITLE);
    model->state_color = lv_color_hex(EVENT_TILE_STATE);
    model->has_ripple_color = true;
    model->ripple_color = lv_color_hex(0xD9D9D9);

    if (entity_handler_snapshot_is_service_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon_color = lv_color_hex(0x6f6f6f);
        // model->icon_circle_background_color = lv_color_hex(0x2c2c2c);
    } else {
        ha_datetime_format_relative_now(snapshot->state,
                                        model->state_text,
                                        sizeof(model->state_text));
        time_t state_time = 0;
        if (ha_datetime_parse_epoch(snapshot->state, &state_time) == ESP_OK) {
            model->state_text_type = ENTITY_STATE_TEXT_RELATIVE_TIME;
            model->state_time = state_time;
        }
    }
    return ESP_OK;
}

static void event_more_info_close_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) {
        context->close(context->close_user_data);
    }
}

static lv_obj_t *event_more_info_build_page(
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
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));

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
                        event_more_info_close_event_cb,
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

static void event_format_state(const ha_entity_snapshot_t *snapshot,
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

    entity_handler_copy_snapshot_state(output, output_size, snapshot);
}

static esp_err_t event_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->close) {
        return ESP_ERR_INVALID_ARG;
    }
    lv_obj_t *main_ctn = event_more_info_build_page(context);

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
    lv_label_set_text(tile_icon, event_get_icon(&context->snapshot));
    lv_obj_set_style_text_font(tile_icon, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(tile_icon, lv_color_hex(0x44739e), 0);

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

    // lv_obj_t *tile_content_cur_state = lv_label_create(tile_content_ctx);
    // lv_obj_set_style_text_font(tile_content_cur_state, &ha_font_misan_regular_16, 0);
    // lv_obj_set_style_text_color(tile_content_cur_state, lv_color_white(), 0);
    // char cur_state_text[ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
    // event_format_state(&context->snapshot,
    //                 cur_state_text,
    //                 sizeof(cur_state_text));
    // lv_label_set_text(tile_content_cur_state, cur_state_text);

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
    
    event_history_prepare(context);
    event_history_render(main_ctn);

    return ESP_OK;
}

const entity_handler_t event_entity_handler = {
    .domain = "event",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_DOORBELL,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_NOTHING },
    },
    .handle_action = event_handle_action,
    .render_tile = event_render_tile,
    .render_more_info = event_render_more_info,
};