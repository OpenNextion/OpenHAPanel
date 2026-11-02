#include "ui_i18n.h"
#include "ha_tile_card.h"

#include "lvgl_private.h"
#include "../../assets/ha_ui_assets.h"
#include "ha_relative_time_label.h"

#define MY_CLASS (&ha_tile_card_class)
#define HA_TILE_CARD_WIDTH  280
#define HA_TILE_CARD_ICON_SIZE 40
#define HA_TILE_CARD_PRESSED_OPA LV_OPA_70

typedef struct {
    lv_obj_t obj;
    lv_obj_t *icon_circle;
    lv_obj_t *icon_label;
    lv_obj_t *icon_badge;
    lv_obj_t *icon_badge_label;
    lv_obj_t *unavailable_icon;
    lv_obj_t *title_label;
    lv_obj_t *state_icon_label;
    lv_obj_t *state_split_line;
    lv_obj_t *state_label;
    lv_obj_t *features_container;
    ha_tile_card_icon_click_cb_t icon_click_cb;
    void *icon_click_user_data;
    ha_tile_card_body_click_cb_t body_click_cb;
    void *body_click_user_data;
    ha_tile_card_body_hold_cb_t body_hold_cb;
    void *body_hold_user_data;
    bool body_long_pressed;
} ha_tile_card_t;

static void ha_tile_card_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ha_tile_card_event(const lv_obj_class_t *class_p, lv_event_t *event);
static void ha_tile_card_icon_event_cb(lv_event_t *event);
static void apply_clear_style(lv_obj_t *obj);

lv_obj_t *ha_tile_card_add_feature_host(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    if (!card->features_container) {
        card->features_container = lv_obj_create(obj);
        if (!card->features_container) {
            return NULL;
        }
        apply_clear_style(card->features_container);
        lv_obj_remove_flag(card->features_container, LV_OBJ_FLAG_SCROLLABLE |
                                                  LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(card->features_container, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(card->features_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_gap(card->features_container, 10, 0);
    }
    lv_obj_t *host = lv_obj_create(card->features_container);
    if (!host) {
        return NULL;
    }
    apply_clear_style(host);
    lv_obj_remove_flag(host, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(host, LV_PCT(100), 56);
    return host;
}

void ha_tile_card_remove_feature_host(lv_obj_t *obj, lv_obj_t *host)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    if (!card->features_container || !host ||
        lv_obj_get_parent(host) != card->features_container) {
        return;
    }
    lv_obj_delete(host);
    if (lv_obj_get_child_count(card->features_container) == 0) {
        lv_obj_delete(card->features_container);
        card->features_container = NULL;
    }
}

const lv_obj_class_t ha_tile_card_class = {
    .constructor_cb = ha_tile_card_constructor,
    .event_cb = ha_tile_card_event,
    .width_def = HA_TILE_CARD_WIDTH,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(ha_tile_card_t),
    .base_class = &lv_obj_class,
    .name = "ha_tile_card",
};

lv_obj_t *ha_tile_card_create(lv_obj_t *parent)
{
    LV_LOG_INFO("begin");

    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    if (!obj) {
        return NULL;
    }

    lv_obj_class_init_obj(obj);
    return obj;
}

void ha_tile_card_set_title(lv_obj_t *obj, const char *title)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    ui_i18n_label_set_text(card->title_label, title ? title : "");
}

void ha_tile_card_set_state_text(lv_obj_t *obj, const char *state_text)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    ha_relative_time_label_unbind(card->state_label);
    ui_i18n_label_set_text(card->state_label, state_text ? state_text : "");
}

esp_err_t ha_tile_card_set_relative_state_time(lv_obj_t *obj,
                                               time_t event_epoch)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    return ha_relative_time_label_bind(card->state_label, event_epoch);
}

esp_err_t ha_tile_card_set_elapsed_state_time(lv_obj_t *obj,
                                              time_t event_epoch)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    return ha_relative_time_label_bind_elapsed(card->state_label, event_epoch);
}

void ha_tile_card_set_icon(lv_obj_t *obj, const char *icon)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    ui_i18n_label_set_text(card->icon_label, icon ? icon : "");
}

void ha_tile_card_set_icon_badge(lv_obj_t *obj,
                                 const char *icon,
                                 lv_color_t background_color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;

    if (icon) {
        ui_i18n_label_set_text(card->icon_badge_label, icon);
        lv_obj_center(card->icon_badge_label);
        lv_obj_set_style_bg_color(card->icon_badge, background_color, 0);
        lv_obj_remove_flag(card->icon_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(card->icon_badge, LV_OBJ_FLAG_HIDDEN);
    }
}

void ha_tile_card_set_unavailable(lv_obj_t *obj, bool unavailable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;

    if (unavailable) {
        lv_obj_remove_flag(card->unavailable_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(card->unavailable_icon);
    }
    else {
        lv_obj_add_flag(card->unavailable_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void ha_tile_card_set_icon_clickable(lv_obj_t *obj, bool clickable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;

    lv_obj_set_style_bg_opa(card->icon_circle, clickable ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (clickable) {
        lv_obj_add_flag(card->icon_circle, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        lv_obj_remove_flag(card->icon_circle, LV_OBJ_FLAG_CLICKABLE);
    }
}

void ha_tile_card_set_icon_click_cb(lv_obj_t *obj,
                                    ha_tile_card_icon_click_cb_t callback,
                                    void *user_data)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    card->icon_click_cb = callback;
    card->icon_click_user_data = user_data;
}

void ha_tile_card_set_body_clickable(lv_obj_t *obj, bool clickable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    if (clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
}

void ha_tile_card_set_body_click_cb(lv_obj_t *obj,
                                    ha_tile_card_body_click_cb_t callback,
                                    void *user_data)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    card->body_click_cb = callback;
    card->body_click_user_data = user_data;
}

void ha_tile_card_set_body_hold_cb(lv_obj_t *obj,
                                   ha_tile_card_body_hold_cb_t callback,
                                   void *user_data)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    card->body_hold_cb = callback;
    card->body_hold_user_data = user_data;
}

void ha_tile_card_set_background_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_obj_set_style_bg_color(obj, color, 0);
}

void ha_tile_card_set_icon_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    lv_obj_set_style_text_color(card->icon_label, color, 0);
}

void ha_tile_card_set_icon_circle_background_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    lv_obj_set_style_bg_color(card->icon_circle, color, 0);
}

void ha_tile_card_set_title_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    lv_obj_set_style_text_color(card->title_label, color, 0);
}

void ha_tile_card_set_state_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    lv_obj_set_style_text_color(card->state_icon_label, color, 0);
    lv_obj_set_style_bg_color(card->state_split_line, color, 0);
    lv_obj_set_style_text_color(card->state_label, color, 0);
}

void ha_tile_card_set_state_icon(lv_obj_t *obj, const char *icon)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    ui_i18n_label_set_text(card->state_icon_label, icon ? icon : "");
}

void ha_tile_card_set_state_icon_visible(lv_obj_t *obj, bool visible)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;

    if (visible) {
        lv_obj_remove_flag(card->state_icon_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(card->state_split_line, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(card->state_icon_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card->state_split_line, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ha_tile_card_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    card->icon_click_cb = NULL;
    card->icon_click_user_data = NULL;
    card->body_click_cb = NULL;
    card->body_click_user_data = NULL;
    card->body_hold_cb = NULL;
    card->body_hold_user_data = NULL;
    card->body_long_pressed = false;

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_radius(obj, 12, 0);
    // lv_obj_set_style_clip_corner(obj, true, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1E2228), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(obj, HA_TILE_CARD_PRESSED_OPA, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_pad_ver(obj, 8, 0);
    lv_obj_set_style_pad_hor(obj, 12, 0);
    lv_obj_set_style_pad_gap(obj, 10, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(obj,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *tile_card = lv_obj_create(obj);
    lv_obj_remove_style_all(tile_card);
    lv_obj_remove_flag(tile_card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_layout(tile_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tile_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile_card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tile_card, 0, 0);
    lv_obj_set_style_pad_gap(tile_card, 12, 0);
    lv_obj_set_size(tile_card, LV_PCT(100), LV_SIZE_CONTENT);

    card->icon_circle = lv_obj_create(tile_card);
    lv_obj_remove_flag(card->icon_circle,
                       LV_OBJ_FLAG_SCROLLABLE |
                           LV_OBJ_FLAG_CLICKABLE |
                           LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(card->icon_circle, HA_TILE_CARD_ICON_SIZE, HA_TILE_CARD_ICON_SIZE);
    lv_obj_set_style_radius(card->icon_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(card->icon_circle, 0, 0);
    lv_obj_set_style_bg_color(card->icon_circle, lv_color_hex(0xECEFF1), 0);
    lv_obj_set_style_bg_opa(card->icon_circle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(card->icon_circle, 0, 0);
    lv_obj_add_event_cb(card->icon_circle,
                        ha_tile_card_icon_event_cb,
                        LV_EVENT_CLICKED,
                        obj);

    card->icon_label = lv_label_create(card->icon_circle);
    lv_obj_center(card->icon_label);
    lv_obj_set_style_text_color(card->icon_label, lv_color_hex(0x4B5058), 0);
    lv_obj_set_style_text_font(card->icon_label, &ha_font_mdi_26, 0);

    card->icon_badge = lv_obj_create(card->icon_circle);
    lv_obj_remove_flag(card->icon_badge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card->icon_badge, 16, 16);
    lv_obj_align(card->icon_badge, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_set_style_radius(card->icon_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(card->icon_badge, 0, 0);
    lv_obj_set_style_bg_opa(card->icon_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card->icon_badge, 0, 0);
    card->icon_badge_label = lv_label_create(card->icon_badge);
    lv_obj_set_style_text_font(card->icon_badge_label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(card->icon_badge_label, lv_color_white(), 0);
    lv_obj_set_style_transform_scale_x(card->icon_badge_label, 154, 0);
    lv_obj_set_style_transform_scale_y(card->icon_badge_label, 154, 0);
    lv_obj_set_style_transform_pivot_x(card->icon_badge_label, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(card->icon_badge_label, LV_PCT(50), 0);
    lv_obj_center(card->icon_badge_label);
    lv_obj_add_flag(card->icon_badge, LV_OBJ_FLAG_HIDDEN);

    card->unavailable_icon = lv_image_create(card->icon_circle);
    lv_image_set_src(card->unavailable_icon, &warning_yellow);
    lv_obj_align(card->unavailable_icon, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_remove_flag(card->unavailable_icon,
                       LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card->unavailable_icon, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *content = lv_obj_create(tile_card);
    apply_clear_style(content);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    card->title_label = lv_label_create(content);
    lv_obj_set_size(card->title_label, LV_PCT(100), lv_font_get_line_height(&ha_font_misan_medium_20));
    lv_label_set_long_mode(card->title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(card->title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(card->title_label, &ha_font_misan_medium_20, 0);
    ui_i18n_label_set_text(card->title_label, "");

    lv_obj_t *state_row = lv_obj_create(content);
    apply_clear_style(state_row);
    lv_obj_remove_flag(state_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(state_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(state_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(state_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(state_row,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(state_row, 0, 0);
    lv_obj_set_style_pad_gap(state_row, 8, 0);

    card->state_icon_label = lv_label_create(state_row);
    lv_obj_set_style_text_color(card->state_icon_label, lv_color_hex(0xA8A8A8), 0);
    lv_obj_set_style_text_font(card->state_icon_label, &ha_font_misan_regular_14, 0);
    lv_obj_set_height(card->state_icon_label, lv_font_get_line_height(&ha_font_misan_regular_14));
    ui_i18n_label_set_text(card->state_icon_label, "");
    lv_obj_add_flag(card->state_icon_label, LV_OBJ_FLAG_HIDDEN);

    card->state_split_line = lv_obj_create(state_row);
    lv_obj_remove_style_all(card->state_split_line);
    lv_obj_set_size(card->state_split_line, 1, LV_PCT(100));
    lv_obj_set_style_border_width(card->state_split_line, 0, 0);
    lv_obj_set_style_bg_color(card->state_split_line, lv_color_hex(0xe1e1e1), 0);
    lv_obj_set_style_bg_opa(card->state_split_line, LV_OPA_30, 0);
    lv_obj_add_flag(card->state_split_line, LV_OBJ_FLAG_HIDDEN);

    card->state_label = lv_label_create(state_row);
    lv_obj_set_flex_grow(card->state_label, 1);
    lv_label_set_long_mode(card->state_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(card->state_label, lv_color_hex(0xA8A8A8), 0);
    lv_obj_set_style_text_font(card->state_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_height(card->state_label, lv_font_get_line_height(&ha_font_misan_regular_16));
    ui_i18n_label_set_text(card->state_label, "");

    LV_TRACE_OBJ_CREATE("finished");
}

static void ha_tile_card_event(const lv_obj_class_t *class_p, lv_event_t *event)
{
    LV_UNUSED(class_p);
    if (lv_obj_event_base(MY_CLASS, event) != LV_RESULT_OK) {
        return;
    }

    ha_tile_card_t *card = (ha_tile_card_t *)lv_event_get_target_obj(event);

    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        card->body_long_pressed = false;
        break;
    case LV_EVENT_CLICKED:
        if (!card->body_long_pressed && card->body_click_cb) {
            card->body_click_cb(&card->obj, card->body_click_user_data);
        }
        break;
    case LV_EVENT_LONG_PRESSED:
        card->body_long_pressed = true;
        if (card->body_hold_cb) {
            card->body_hold_cb(&card->obj, card->body_hold_user_data);
        }
        break;
    default:
        break;
    }
}

static void ha_tile_card_icon_event_cb(lv_event_t *event)
{
    lv_event_stop_bubbling(event);

    lv_obj_t *obj = lv_event_get_user_data(event);
    if (!obj) {
        return;
    }

    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    if (card->icon_click_cb) {
        card->icon_click_cb(obj, card->icon_click_user_data);
    }
}

static void apply_clear_style(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}
