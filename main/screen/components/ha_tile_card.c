#include "ha_tile_card.h"

#include "lvgl_private.h"
#include "../../assets/ha_ui_assets.h"
#include "ha_relative_time_label.h"

#define MY_CLASS (&ha_tile_card_class)
#define HA_TILE_CARD_WIDTH  280
#define HA_TILE_CARD_ICON_SIZE 40
#define HA_TILE_CARD_RIPPLE_EXPAND_MS 360
#define HA_TILE_CARD_RIPPLE_FADE_MS   220
#define HA_TILE_CARD_RIPPLE_OPA       34

typedef struct {
    lv_obj_t obj;
    lv_obj_t *icon_circle;
    lv_obj_t *icon_label;
    lv_obj_t *unavailable_icon;
    lv_obj_t *title_label;
    lv_obj_t *state_icon_label;
    lv_obj_t *state_split_line;
    lv_obj_t *state_label;
    ha_tile_card_icon_click_cb_t icon_click_cb;
    void *icon_click_user_data;
    ha_tile_card_body_click_cb_t body_click_cb;
    void *body_click_user_data;
    ha_tile_card_body_hold_cb_t body_hold_cb;
    void *body_hold_user_data;
    bool body_long_pressed;
    lv_color_t ripple_color;
    lv_point_t ripple_origin;
    int32_t ripple_progress;
    int32_t ripple_opa;
} ha_tile_card_t;

static void ha_tile_card_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ha_tile_card_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ha_tile_card_event(const lv_obj_class_t *class_p, lv_event_t *event);
static void ha_tile_card_icon_event_cb(lv_event_t *event);
static void ha_tile_card_ripple_start(ha_tile_card_t *card);
static void ha_tile_card_ripple_release(ha_tile_card_t *card);
static void ha_tile_card_ripple_draw(ha_tile_card_t *card, lv_event_t *event);
static void ha_tile_card_ripple_set_progress(void *var, int32_t value);
static void ha_tile_card_ripple_set_opa(void *var, int32_t value);
static void apply_clear_style(lv_obj_t *obj);

const lv_obj_class_t ha_tile_card_class = {
    .constructor_cb = ha_tile_card_constructor,
    .destructor_cb = ha_tile_card_destructor,
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
    lv_label_set_text(card->title_label, title ? title : "");
}

void ha_tile_card_set_state_text(lv_obj_t *obj, const char *state_text)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    ha_relative_time_label_unbind(card->state_label);
    lv_label_set_text(card->state_label, state_text ? state_text : "");
}

esp_err_t ha_tile_card_set_relative_state_time(lv_obj_t *obj,
                                               time_t event_epoch)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    return ha_relative_time_label_bind(card->state_label, event_epoch);
}

void ha_tile_card_set_icon(lv_obj_t *obj, const char *icon)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    lv_label_set_text(card->icon_label, icon ? icon : "");
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
    ha_tile_card_t *card = (ha_tile_card_t *)obj;

    if (clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        ha_tile_card_stop_ripple(obj);
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

void ha_tile_card_set_ripple_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    card->ripple_color = color;
}

void ha_tile_card_stop_ripple(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    lv_anim_delete(card, ha_tile_card_ripple_set_progress);
    lv_anim_delete(card, ha_tile_card_ripple_set_opa);
    card->ripple_progress = 0;
    card->ripple_opa = 0;
    lv_obj_invalidate(obj);
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
    lv_label_set_text(card->state_icon_label, icon ? icon : "");
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
    card->ripple_color = lv_color_hex(0xFFFFFF);
    card->ripple_progress = 0;
    card->ripple_opa = 0;

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_radius(obj, 12, 0);
    // lv_obj_set_style_clip_corner(obj, true, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1E2228), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
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
    lv_label_set_text(card->title_label, "");

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
    lv_obj_set_style_text_font(card->state_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_height(card->state_icon_label, lv_font_get_line_height(&lv_font_montserrat_14));
    lv_label_set_text(card->state_icon_label, "");
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
    lv_label_set_text(card->state_label, "");

    LV_TRACE_OBJ_CREATE("finished");
}

static void ha_tile_card_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    ha_tile_card_t *card = (ha_tile_card_t *)obj;
    lv_anim_delete(card, ha_tile_card_ripple_set_progress);
    lv_anim_delete(card, ha_tile_card_ripple_set_opa);
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
        ha_tile_card_ripple_start(card);
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        ha_tile_card_ripple_release(card);
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
    case LV_EVENT_DRAW_POST:
        ha_tile_card_ripple_draw(card, event);
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

static void ha_tile_card_ripple_start(ha_tile_card_t *card)
{
    lv_indev_t *indev = lv_indev_active();
    lv_area_t area;
    lv_obj_get_coords(&card->obj, &area);

    card->ripple_origin.x = lv_area_get_width(&area) / 2;
    card->ripple_origin.y = lv_area_get_height(&area) / 2;
    if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        card->ripple_origin.x = point.x - area.x1;
        card->ripple_origin.y = point.y - area.y1;
    }

    lv_anim_delete(card, ha_tile_card_ripple_set_progress);
    lv_anim_delete(card, ha_tile_card_ripple_set_opa);
    card->ripple_progress = 0;
    card->ripple_opa = HA_TILE_CARD_RIPPLE_OPA;

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, card);
    lv_anim_set_values(&animation, 0, 255);
    lv_anim_set_duration(&animation, HA_TILE_CARD_RIPPLE_EXPAND_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, ha_tile_card_ripple_set_progress);
    lv_anim_start(&animation);
}

static void ha_tile_card_ripple_release(ha_tile_card_t *card)
{
    if (card->ripple_opa <= 0) {
        return;
    }

    lv_anim_delete(card, ha_tile_card_ripple_set_opa);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, card);
    lv_anim_set_values(&animation, card->ripple_opa, 0);
    lv_anim_set_duration(&animation, HA_TILE_CARD_RIPPLE_FADE_MS);
    lv_anim_set_exec_cb(&animation, ha_tile_card_ripple_set_opa);
    lv_anim_start(&animation);
}

static void ha_tile_card_ripple_draw(ha_tile_card_t *card, lv_event_t *event)
{
    if (card->ripple_opa <= 0 || card->ripple_progress <= 0 ||
        lv_obj_get_screen(&card->obj) != lv_screen_active()) {
        return;
    }

    lv_area_t card_area;
    lv_obj_get_coords(&card->obj, &card_area);
    const int32_t width = lv_area_get_width(&card_area);
    const int32_t height = lv_area_get_height(&card_area);
    const int32_t dx = LV_MAX(card->ripple_origin.x,
                              width - card->ripple_origin.x);
    const int32_t dy = LV_MAX(card->ripple_origin.y,
                              height - card->ripple_origin.y);
    const int32_t radius = LV_MAX(1,
                                  (dx + dy) * card->ripple_progress / 255);
    const int32_t center_x = card_area.x1 + card->ripple_origin.x;
    const int32_t center_y = card_area.y1 + card->ripple_origin.y;
    const lv_area_t ripple_area = {
        .x1 = center_x - radius,
        .y1 = center_y - radius,
        .x2 = center_x + radius,
        .y2 = center_y + radius,
    };

    lv_draw_rect_dsc_t draw_dsc;
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_color = card->ripple_color;
    draw_dsc.bg_opa = (lv_opa_t)card->ripple_opa;
    draw_dsc.radius = LV_RADIUS_CIRCLE;
    draw_dsc.border_width = 0;
    lv_draw_rect(lv_event_get_layer(event), &draw_dsc, &ripple_area);
}

static void ha_tile_card_ripple_set_progress(void *var, int32_t value)
{
    ha_tile_card_t *card = var;
    card->ripple_progress = value;
    lv_obj_invalidate(&card->obj);
}

static void ha_tile_card_ripple_set_opa(void *var, int32_t value)
{
    ha_tile_card_t *card = var;
    card->ripple_opa = value;
    lv_obj_invalidate(&card->obj);
}

static void apply_clear_style(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}