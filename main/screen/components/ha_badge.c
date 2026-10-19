#include "ha_badge.h"

#include "assets/ha_ui_assets.h"
#include "lvgl_private.h"

#define MY_CLASS (&ha_badge_class)
#define BADGE_HEIGHT 30
#define BADGE_RIPPLE_OPA 34

typedef struct {
    lv_obj_t obj;
    lv_obj_t *icon_label;
    lv_obj_t *text_label;
    ha_badge_action_cb_t click_cb;
    void *click_user_data;
    ha_badge_action_cb_t hold_cb;
    void *hold_user_data;
    lv_point_t ripple_origin;
    int32_t ripple_progress;
    int32_t ripple_opa;
} ha_badge_t;

static void ha_badge_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ha_badge_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ha_badge_event(const lv_obj_class_t *class_p, lv_event_t *event);
static void ha_badge_ripple_start(ha_badge_t *badge);
static void ha_badge_ripple_release(ha_badge_t *badge);
static void ha_badge_ripple_draw(ha_badge_t *badge, lv_event_t *event);
static void ha_badge_ripple_set_progress(void *var, int32_t value);
static void ha_badge_ripple_set_opa(void *var, int32_t value);

const lv_obj_class_t ha_badge_class = {
    .constructor_cb = ha_badge_constructor,
    .destructor_cb = ha_badge_destructor,
    .event_cb = ha_badge_event,
    .width_def = LV_SIZE_CONTENT,
    .height_def = BADGE_HEIGHT,
    .instance_size = sizeof(ha_badge_t),
    .base_class = &lv_obj_class,
    .name = "ha_badge",
};

lv_obj_t *ha_badge_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    if (obj) {
        lv_obj_class_init_obj(obj);
    }
    return obj;
}

void ha_badge_set_icon(lv_obj_t *obj, const char *icon)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    lv_label_set_text(badge->icon_label, icon ? icon : "");
}

void ha_badge_set_text(lv_obj_t *obj, const char *text)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    lv_label_set_text(badge->text_label, text ? text : "");
}

void ha_badge_set_icon_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    lv_obj_set_style_text_color(badge->icon_label, color, 0);
}

void ha_badge_set_text_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    lv_obj_set_style_text_color(badge->text_label, color, 0);
}

void ha_badge_set_body_clickable(lv_obj_t *obj, bool clickable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        ha_badge_stop_ripple(obj);
    }
}

void ha_badge_set_body_click_cb(lv_obj_t *obj, ha_badge_action_cb_t callback,
                                void *user_data)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    badge->click_cb = callback;
    badge->click_user_data = user_data;
}

void ha_badge_set_body_hold_cb(lv_obj_t *obj, ha_badge_action_cb_t callback,
                               void *user_data)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    badge->hold_cb = callback;
    badge->hold_user_data = user_data;
}

void ha_badge_stop_ripple(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    lv_anim_delete(badge, ha_badge_ripple_set_progress);
    lv_anim_delete(badge, ha_badge_ripple_set_opa);
    badge->ripple_progress = 0;
    badge->ripple_opa = 0;
    lv_obj_invalidate(obj);
}

static void ha_badge_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    ha_badge_t *badge = (ha_badge_t *)obj;
    badge->click_cb = NULL;
    badge->hold_cb = NULL;
    badge->ripple_progress = 0;
    badge->ripple_opa = 0;

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, BADGE_HEIGHT);
    lv_obj_set_style_max_width(obj, LV_HOR_RES - 40, 0);
    lv_obj_set_style_radius(obj, BADGE_HEIGHT / 2, 0);
    // lv_obj_set_style_clip_corner(obj, true, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x999999), 0);
    lv_obj_set_style_bg_opa(obj, 0x33, 0);
    lv_obj_set_style_pad_hor(obj, 8, 0);
    lv_obj_set_style_pad_ver(obj, 0, 0);
    lv_obj_set_style_pad_column(obj, 4, 0);
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    badge->icon_label = lv_label_create(obj);
    lv_obj_set_size(badge->icon_label, 20, 20);
    lv_obj_set_style_text_font(badge->icon_label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_align(badge->icon_label, LV_TEXT_ALIGN_CENTER, 0);

    badge->text_label = lv_label_create(obj);
    lv_label_set_long_mode(badge->text_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(badge->text_label, LV_HOR_RES - 80, 0);
    lv_obj_set_style_text_color(badge->text_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(badge->text_label, &ha_font_misan_regular_14, 0);
}

static void ha_badge_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    ha_badge_t *badge = (ha_badge_t *)obj;
    lv_anim_delete(badge, ha_badge_ripple_set_progress);
    lv_anim_delete(badge, ha_badge_ripple_set_opa);
}

static void ha_badge_event(const lv_obj_class_t *class_p, lv_event_t *event)
{
    LV_UNUSED(class_p);
    if (lv_obj_event_base(MY_CLASS, event) != LV_RESULT_OK) {
        return;
    }

    ha_badge_t *badge = (ha_badge_t *)lv_event_get_target_obj(event);
    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        ha_badge_ripple_start(badge);
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        ha_badge_ripple_release(badge);
        break;
    case LV_EVENT_SHORT_CLICKED:
        if (badge->click_cb) {
            badge->click_cb(&badge->obj, badge->click_user_data);
        }
        break;
    case LV_EVENT_LONG_PRESSED:
        if (badge->hold_cb) {
            badge->hold_cb(&badge->obj, badge->hold_user_data);
        }
        break;
    case LV_EVENT_DRAW_POST:
        ha_badge_ripple_draw(badge, event);
        break;
    default:
        break;
    }
}

static void ha_badge_ripple_start(ha_badge_t *badge)
{
    lv_area_t area;
    lv_obj_get_coords(&badge->obj, &area);
    badge->ripple_origin.x = lv_area_get_width(&area) / 2;
    badge->ripple_origin.y = lv_area_get_height(&area) / 2;
    lv_indev_t *indev = lv_indev_active();
    if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        badge->ripple_origin.x = point.x - area.x1;
        badge->ripple_origin.y = point.y - area.y1;
    }

    lv_anim_delete(badge, ha_badge_ripple_set_progress);
    lv_anim_delete(badge, ha_badge_ripple_set_opa);
    badge->ripple_progress = 0;
    badge->ripple_opa = BADGE_RIPPLE_OPA;

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, badge);
    lv_anim_set_values(&animation, 0, 255);
    lv_anim_set_duration(&animation, 360);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, ha_badge_ripple_set_progress);
    lv_anim_start(&animation);
}

static void ha_badge_ripple_release(ha_badge_t *badge)
{
    if (badge->ripple_opa <= 0) {
        return;
    }
    lv_anim_delete(badge, ha_badge_ripple_set_opa);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, badge);
    lv_anim_set_values(&animation, badge->ripple_opa, 0);
    lv_anim_set_duration(&animation, 220);
    lv_anim_set_exec_cb(&animation, ha_badge_ripple_set_opa);
    lv_anim_start(&animation);
}

static void ha_badge_ripple_draw(ha_badge_t *badge, lv_event_t *event)
{
    if (badge->ripple_opa <= 0 || badge->ripple_progress <= 0 ||
        lv_obj_get_screen(&badge->obj) != lv_screen_active()) {
        return;
    }
    lv_area_t area;
    lv_obj_get_coords(&badge->obj, &area);
    const int32_t width = lv_area_get_width(&area);
    const int32_t height = lv_area_get_height(&area);
    const int32_t dx = LV_MAX(badge->ripple_origin.x,
                              width - badge->ripple_origin.x);
    const int32_t dy = LV_MAX(badge->ripple_origin.y,
                              height - badge->ripple_origin.y);
    const int32_t radius = LV_MAX(1, (dx + dy) * badge->ripple_progress / 255);
    const int32_t x = area.x1 + badge->ripple_origin.x;
    const int32_t y = area.y1 + badge->ripple_origin.y;
    const lv_area_t ripple_area = {
        .x1 = x - radius, .y1 = y - radius,
        .x2 = x + radius, .y2 = y + radius,
    };
    lv_draw_rect_dsc_t draw_dsc;
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_color = lv_color_hex(0xFFFFFF);
    draw_dsc.bg_opa = (lv_opa_t)badge->ripple_opa;
    draw_dsc.radius = LV_RADIUS_CIRCLE;
    draw_dsc.border_width = 0;
    lv_draw_rect(lv_event_get_layer(event), &draw_dsc, &ripple_area);
}

static void ha_badge_ripple_set_progress(void *var, int32_t value)
{
    ha_badge_t *badge = var;
    badge->ripple_progress = value;
    lv_obj_invalidate(&badge->obj);
}

static void ha_badge_ripple_set_opa(void *var, int32_t value)
{
    ha_badge_t *badge = var;
    badge->ripple_opa = value;
    lv_obj_invalidate(&badge->obj);
}