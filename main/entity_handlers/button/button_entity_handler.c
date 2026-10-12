#include "button_entity_handler.h"

#include <stdio.h>
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

#define SCREEN_DEFAULT_CONTENT_WIDTH 280
#define BUTTON_HISTORY_DAYS 3
#define BUTTON_HISTORY_MAX_RECORDS 14
#define BUTTON_HISTORY_PRESSED_STATE "pressed"
#define BUTTON_ACTION_COLOR 0x039BE5
#define BUTTON_ACTION_DISABLED_COLOR 0x075F91
#define BUTTON_ACTION_PRESSED_BG_COLOR 0x193E63

static const char *TAG = "button_entity_handler";

typedef struct {
    time_t epoch;
    char state[HA_ENTITY_STORE_STATE_MAX];
} button_history_record_t;

typedef struct {
    lv_obj_t *screen;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    char entity_state[HA_ENTITY_STORE_STATE_MAX];
    int request_id;
    bool loading;
    bool loaded;
    bool failed;
    size_t record_count;
    button_history_record_t records[BUTTON_HISTORY_MAX_RECORDS];
} button_history_cache_t;

static button_history_cache_t s_button_history;

static void button_history_prepare(
    const entity_more_info_context_t *context);
static void button_history_request(const char *entity_id);
static void button_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx);
static bool button_history_parse_result(const cJSON *result);
static bool button_history_parse_epoch(const cJSON *item,
                                       time_t *epoch_out);
static void button_history_insert(time_t epoch, const char *state);
static void button_history_observe_state(
    const ha_entity_snapshot_t *snapshot);
static void button_history_screen_delete_cb(lv_event_t *event);
static void button_history_reset(void);
static void button_history_render(lv_obj_t *parent);

static void button_history_prepare(
    const entity_more_info_context_t *context)
{
    const char *entity_id = context->snapshot.entity_id;
    const bool entity_changed =
        strcmp(s_button_history.entity_id, entity_id) != 0;

    if (entity_changed) {
        button_history_reset();
        snprintf(s_button_history.entity_id,
                 sizeof(s_button_history.entity_id),
                 "%s",
                 entity_id);
        snprintf(s_button_history.entity_state,
                 sizeof(s_button_history.entity_state),
                 "%s",
                 context->snapshot.state);
    }

    if (s_button_history.screen != context->screen) {
        s_button_history.screen = context->screen;
        if (s_button_history.failed) {
            s_button_history.failed = false;
        }
        lv_obj_add_event_cb(context->screen,
                            button_history_screen_delete_cb,
                            LV_EVENT_DELETE,
                            NULL);
    }

    button_history_observe_state(&context->snapshot);

    button_history_request(entity_id);
}

static void button_history_request(const char *entity_id)
{
    if (!entity_id || !entity_id[0] || s_button_history.loading ||
        s_button_history.loaded || s_button_history.failed) {
        return;
    }

    const time_t end_time = time(NULL);
    if (end_time <= 0) {
        s_button_history.failed = true;
        return;
    }

    s_button_history.loading = true;
    int request_id = 0;
    const esp_err_t err = app_ha_get_entity_history(
        entity_id,
        end_time - BUTTON_HISTORY_DAYS * 24 * 60 * 60,
        end_time,
        button_history_result_cb,
        &s_button_history,
        &request_id);
    if (err != ESP_OK) {
        s_button_history.loading = false;
        s_button_history.failed = true;
        ESP_LOGW(TAG,
                 "Failed to request history for %s: %s",
                 entity_id,
                 esp_err_to_name(err));
        return;
    }
    s_button_history.request_id = request_id;
}

static void button_history_result_cb(int request_id,
                                     esp_err_t err,
                                     const cJSON *result,
                                     const cJSON *error,
                                     void *user_ctx)
{
    (void)error;
    button_history_cache_t *history = user_ctx;
    if (history != &s_button_history || !lvgl_port_lock(0)) {
        return;
    }

    if (request_id != history->request_id) {
        lvgl_port_unlock();
        return;
    }

    history->loading = false;
    history->loaded = err == ESP_OK && button_history_parse_result(result);
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

static bool button_history_parse_result(const cJSON *result)
{
    if (!cJSON_IsObject(result)) {
        return false;
    }

    const cJSON *states = cJSON_GetObjectItemCaseSensitive(
        result,
        s_button_history.entity_id);
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
        if (!cJSON_IsString(state) || !state->valuestring) {
            continue;
        }

        if (ha_datetime_parse_epoch(state->valuestring, &epoch) == ESP_OK) {
            button_history_insert(epoch, BUTTON_HISTORY_PRESSED_STATE);
        }
        else if ((strcmp(state->valuestring, "unavailable") == 0 ||
                  strcmp(state->valuestring, "unknown") == 0) &&
                 button_history_parse_epoch(item, &epoch)) {
            button_history_insert(epoch, state->valuestring);
        }
    }
    return true;
}

static bool button_history_parse_epoch(const cJSON *item,
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

static void button_history_insert(time_t epoch, const char *state)
{
    if (epoch <= 0 || !state || !state[0]) {
        return;
    }

    size_t position = 0;
    while (position < s_button_history.record_count &&
           s_button_history.records[position].epoch > epoch) {
        position++;
    }
    if (position < s_button_history.record_count &&
        s_button_history.records[position].epoch == epoch) {
        snprintf(s_button_history.records[position].state,
                 sizeof(s_button_history.records[position].state),
                 "%s",
                 state);
        return;
    }
    if (position >= BUTTON_HISTORY_MAX_RECORDS) {
        return;
    }

    const size_t new_count =
        s_button_history.record_count < BUTTON_HISTORY_MAX_RECORDS
            ? s_button_history.record_count + 1
            : BUTTON_HISTORY_MAX_RECORDS;
    for (size_t i = new_count - 1; i > position; i--) {
        s_button_history.records[i] = s_button_history.records[i - 1];
    }
    s_button_history.records[position].epoch = epoch;
    snprintf(s_button_history.records[position].state,
             sizeof(s_button_history.records[position].state),
             "%s",
             state);
    s_button_history.record_count = new_count;
}

static void button_history_observe_state(
    const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot || !s_button_history.entity_id[0] ||
        strcmp(s_button_history.entity_id, snapshot->entity_id) != 0 ||
        strcmp(s_button_history.entity_state, snapshot->state) == 0) {
        return;
    }

    snprintf(s_button_history.entity_state,
             sizeof(s_button_history.entity_state),
             "%s",
             snapshot->state);

    time_t state_epoch = 0;
    if (ha_datetime_parse_epoch(snapshot->state, &state_epoch) == ESP_OK) {
        button_history_insert(state_epoch, BUTTON_HISTORY_PRESSED_STATE);
    }
    else if (strcmp(snapshot->state, "unavailable") == 0 ||
             strcmp(snapshot->state, "unknown") == 0) {
        const char *changed_at = snapshot->last_changed[0]
                                     ? snapshot->last_changed
                                     : snapshot->last_updated;
        if (ha_datetime_parse_epoch(changed_at, &state_epoch) == ESP_OK) {
            button_history_insert(state_epoch, snapshot->state);
        }
    }
}

static void button_history_screen_delete_cb(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_button_history.screen) {
        s_button_history.screen = NULL;
    }
}

static void button_history_reset(void)
{
    memset(&s_button_history, 0, sizeof(s_button_history));
    s_button_history.request_id = -1;
}

static void button_history_render(lv_obj_t *parent)
{
    const char *message = NULL;
    if (s_button_history.loading && s_button_history.record_count == 0) {
        message = "Loading history...";
    }
    else if (s_button_history.failed && s_button_history.record_count == 0) {
        message = "History unavailable";
    }
    else if (s_button_history.record_count == 0) {
        message = "No history";
    }

    ha_history_list_item_t items[BUTTON_HISTORY_MAX_RECORDS] = {0};
    for (size_t i = 0; i < s_button_history.record_count; i++) {
        const char *state = s_button_history.records[i].state;
        const char *text = state;
        if (strcmp(state, BUTTON_HISTORY_PRESSED_STATE) == 0) {
            text = "Pressed";
        }
        else if (strcmp(state, "unavailable") == 0) {
            text = "Unavailable";
        }
        else if (strcmp(state, "unknown") == 0) {
            text = "Unknown";
        }
        items[i].epoch = s_button_history.records[i].epoch;
        items[i].text = text;
    }

    const ha_history_list_config_t config = {
        .title = "History",
        .items = items,
        .item_count = s_button_history.record_count,
        .message = message,
    };
    if (!ha_history_list_create(parent, &config)) {
        ESP_LOGW(TAG, "Failed to create button history list");
    }
}

static bool button_press(const char *entity_id)
{
    if (!entity_id || !entity_id[0]) {
        return false;
    }

    esp_err_t err = app_ha_call_entity_service("button",
                                               "press",
                                               entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to call button.press for %s: %s",
                 entity_id,
                 esp_err_to_name(err));
    }
    return true;
}

static bool button_handle_action(const entity_action_context_t *context)
{
    if (!context) {
        return false;
    }

    if (entity_handler_handle_more_info_action(context)) {
        return true;
    }

    const screen_action_t *action = entity_handler_get_action(context);
    if (!action || action->action != SCREEN_ACTION_TOGGLE ||
        !context->entity_id) {
        return false;
    }

    return button_press(context->entity_id);
}

static void button_more_info_close_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) {
        context->close(context->close_user_data);
    }
}

static void button_more_info_press_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (!context || !context->snapshot.entity_id[0]) {
        return;
    }

    button_press(context->snapshot.entity_id);
}

static lv_obj_t *button_more_info_build_page(
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
                        button_more_info_close_event_cb,
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
                        button_more_info_close_event_cb,
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

static esp_err_t button_render_tile(ha_entity_store_t *store,
                                   const ha_entity_snapshot_t *snapshot,
                                   entity_display_model_t *model)
{
    (void)store;
    if (!snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_button_history.entity_id[0] &&
        strcmp(s_button_history.entity_id, snapshot->entity_id) == 0) {
        button_history_observe_state(snapshot);
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = MDI_SYMBOL_GESTURE_TAP_BUTTON;
    model->background_color = lv_color_hex(0x1E2228);
    model->icon_color = lv_color_hex(0x44739e);
    model->icon_circle_background_color = lv_color_hex(0x242d36);
    model->title_color = lv_color_hex(0xFFFFFF);
    model->state_color = lv_color_hex(0x666666);
    model->has_ripple_color = true;
    model->ripple_color = lv_color_hex(0xD9D9D9);

    if (entity_handler_snapshot_is_service_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon_color = lv_color_hex(0x6f6f6f);
        model->icon_circle_background_color = lv_color_hex(0x2c2c2c);
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

static esp_err_t button_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->close) {
        return ESP_ERR_INVALID_ARG;
    }

    lv_obj_t *main_ctn = button_more_info_build_page(context);

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

    const bool unavailable = entity_handler_snapshot_is_service_unavailable(
        &context->snapshot);

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
    lv_label_set_text(tile_icon, MDI_SYMBOL_GESTURE_TAP_BUTTON);
    lv_obj_set_style_text_font(tile_icon, &ha_font_mdi_26, 0);
    lv_obj_set_style_text_color(tile_icon, unavailable ? lv_color_hex(0x6f6f6f) : lv_color_hex(0x44739e), 0);


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
    // 展示实体的friendly_name
    lv_label_set_text(tile_content_title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);

    lv_obj_t *tile_content_state = lv_label_create(tile_content_ctx);
    // 展示实体触发的 相对时间 ha_datetime_format_relative_now
    char state_text[HA_ENTITY_STORE_STATE_MAX] = {0};
    if (unavailable) {
        entity_handler_copy_snapshot_state(state_text,
                                           sizeof(state_text),
                                           &context->snapshot);
    }
    else {
        ha_datetime_format_relative_now(context->snapshot.state,
                                        state_text,
                                        sizeof(state_text));
    }
    lv_label_set_text(tile_content_state, state_text);
    if (!unavailable) {
        time_t state_time = 0;
        if (ha_datetime_parse_epoch(context->snapshot.state,
                                    &state_time) == ESP_OK) {
            ha_relative_time_label_bind(tile_content_state, state_time);
        }
    }
    lv_obj_set_style_text_font(tile_content_state, &ha_font_misan_regular_16, 0);
    lv_obj_set_size(tile_content_state, LV_PCT(100), lv_font_get_line_height(&ha_font_misan_regular_16));
    lv_label_set_long_mode(tile_content_state, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(tile_content_state, lv_color_hex(0x666666), 0);

    lv_obj_t *press_button = lv_button_create(content);
    lv_obj_remove_style_all(press_button);
    lv_obj_add_flag(press_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(press_button, LV_PCT(100), 48);
    lv_obj_set_style_radius(press_button, 12, 0);
    lv_obj_set_style_border_width(press_button, 0, 0);
    lv_obj_set_style_shadow_width(press_button, 0, 0);
    /* Match the scene action: idle and disabled blend into the card, while
     * pressing reveals the blue rounded surface. */
    lv_obj_set_style_bg_color(press_button, lv_color_hex(0x1E2228), 0);
    lv_obj_set_style_bg_opa(press_button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(press_button,
                              lv_color_hex(BUTTON_ACTION_PRESSED_BG_COLOR),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(press_button,
                              lv_color_hex(0x1E2228),
                              LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(press_button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(press_button, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_add_event_cb(press_button,
                        button_more_info_press_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);
    lv_obj_set_style_transform_width(press_button, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(press_button, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_x(press_button, 256, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(press_button, 256, LV_STATE_PRESSED);
    if (unavailable) {
        lv_obj_add_state(press_button, LV_STATE_DISABLED);
    }

    lv_obj_t *button_label = lv_label_create(press_button);
    lv_label_set_text(button_label, "Press");
    lv_obj_set_style_text_font(button_label, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(
        button_label,
        lv_color_hex(unavailable ? BUTTON_ACTION_DISABLED_COLOR
                                 : BUTTON_ACTION_COLOR),
        0);
    lv_obj_center(button_label);

    button_history_prepare(context);
    button_history_render(main_ctn);

    return ESP_OK;
}

const entity_handler_t button_entity_handler = {
    .domain = "button",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_GESTURE_TAP_BUTTON,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_TOGGLE },
    },
    .handle_action = button_handle_action,
    .render_tile = button_render_tile,
    .render_more_info = button_render_more_info,
};
