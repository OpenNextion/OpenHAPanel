#include "scene_entity_handler.h"

#include <time.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_log.h"
#include "screen/components/ha_relative_time_label.h"

#define SCREEN_DEFAULT_CONTENT_WIDTH 280
#define SCENE_ACTION_COLOR 0x039BE5
#define SCENE_ACTION_DISABLED_COLOR 0x075F91
#define SCENE_ACTION_PRESSED_BG_COLOR 0x193E63

static const char *TAG = "scene_entity_handler";

static bool scene_turn_on(const char *entity_id)
{
    if (!entity_id || !entity_id[0]) {
        return false;
    }

    esp_err_t err = app_ha_call_entity_service("scene",
                                               "turn_on",
                                               entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to call scene.turn_on for %s: %s",
                 entity_id,
                 esp_err_to_name(err));
    }
    return true;
}

static bool scene_handle_action(const entity_action_context_t *context)
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

    return scene_turn_on(context->entity_id);
}

static void scene_more_info_close_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) {
        context->close(context->close_user_data);
    }
}

static void scene_more_info_press_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (!context || !context->snapshot.entity_id[0]) {
        return;
    }

    scene_turn_on(context->snapshot.entity_id);
}

static lv_obj_t *scene_more_info_build_page(
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
                        scene_more_info_close_event_cb,
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
                        scene_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_t *content = lv_obj_create(screen);
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
    return content;
}

static esp_err_t scene_render_tile(ha_entity_store_t *store,
                                   const ha_entity_snapshot_t *snapshot,
                                   entity_display_model_t *model)
{
    (void)store;
    if (!snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = MDI_SYMBOL_PALETTE;
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

static esp_err_t scene_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->close) {
        return ESP_ERR_INVALID_ARG;
    }

    lv_obj_t *content = scene_more_info_build_page(context);


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
    lv_label_set_text(tile_icon, MDI_SYMBOL_PALETTE);
    lv_obj_set_style_text_font(tile_icon, &ha_font_mdi_26, 0);
    lv_obj_set_style_text_color(tile_icon, lv_color_hex(0xFFFFFF), 0);
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
    /* The idle action blends into the card. Only the pressed state exposes
     * the blue rounded surface shown by Home Assistant. */
    lv_obj_set_style_bg_color(press_button, lv_color_hex(0x1E2228), 0);
    lv_obj_set_style_bg_opa(press_button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(press_button,
                              lv_color_hex(SCENE_ACTION_PRESSED_BG_COLOR),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(press_button,
                              lv_color_hex(0x1E2228),
                              LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(press_button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(press_button, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_add_event_cb(press_button,
                        scene_more_info_press_event_cb,
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
    lv_label_set_text(button_label, "Active");
    lv_obj_set_style_text_font(button_label, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(
        button_label,
        lv_color_hex(unavailable ? SCENE_ACTION_DISABLED_COLOR
                                 : SCENE_ACTION_COLOR),
        0);
    lv_obj_center(button_label);
    return ESP_OK;
}

const entity_handler_t scene_entity_handler = {
    .domain = "scene",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_PALETTE,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_TOGGLE },
    },
    .handle_action = scene_handle_action,
    .render_tile = scene_render_tile,
    .render_more_info = scene_render_more_info,
};
