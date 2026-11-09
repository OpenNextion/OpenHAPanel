#include "ui_i18n.h"
#include "ha_ui_font.h"
#include "ha_badge.h"

#include "assets/ha_ui_assets.h"
#include "lvgl_private.h"
#include "screen_theme.h"

#define MY_CLASS (&ha_badge_class)
#define BADGE_HEIGHT 30
#define BADGE_LIGHT_BACKGROUND_COLOR 0xFFFFFF
#define BADGE_DARK_BACKGROUND_COLOR 0x1C1C1C
#define BADGE_PRESSED_MIX 31 /* 12% of the badge color */
#define BADGE_BORDER_OPA 31 /* 12% */
#define BADGE_TEXT_COLOR_DARK 0xE4E7EE
#define BADGE_TEXT_COLOR_LIGHT 0x333333
#define BADGE_UNAVAILABLE_TEXT_COLOR_DARK 0x909090
/* HA heading badges render unavailable state as an em dash that inherits
 * --secondary-text-color; the current light-theme default is #5E5E5E. */
#define BADGE_UNAVAILABLE_TEXT_COLOR_LIGHT 0x5E5E5E

typedef struct {
    lv_obj_t obj;
    lv_obj_t *icon_label;
    lv_obj_t *text_label;
    ha_badge_action_cb_t click_cb;
    void *click_user_data;
    ha_badge_action_cb_t hold_cb;
    void *hold_user_data;
    lv_color_t pressed_color;
    bool unavailable;
} ha_badge_t;

static void ha_badge_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ha_badge_event(const lv_obj_class_t *class_p, lv_event_t *event);

const lv_obj_class_t ha_badge_class = {
    .constructor_cb = ha_badge_constructor,
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
    ui_i18n_label_set_text(badge->icon_label, icon ? icon : "");
}

void ha_badge_set_text(lv_obj_t *obj, const char *text)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    ui_i18n_label_set_text(badge->text_label, text ? text : "");
}

void ha_badge_set_icon_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    badge->pressed_color = color;
    lv_obj_set_style_text_color(badge->icon_label, color, 0);
    ha_badge_apply_theme(obj);
}

void ha_badge_set_text_color(lv_obj_t *obj, lv_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    lv_obj_set_style_text_color(badge->text_label, color, 0);
}

void ha_badge_set_unavailable(lv_obj_t *obj, bool unavailable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    badge->unavailable = unavailable;
    ha_badge_apply_theme(obj);
}

void ha_badge_apply_theme(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    ha_badge_t *badge = (ha_badge_t *)obj;
    const bool light = screen_theme_get() == SCREEN_THEME_LIGHT;
    const uint32_t text_color = light ? BADGE_TEXT_COLOR_LIGHT : BADGE_TEXT_COLOR_DARK;
    const lv_color_t background_color = lv_color_hex(
        light ? BADGE_LIGHT_BACKGROUND_COLOR : BADGE_DARK_BACKGROUND_COLOR);

    lv_obj_set_style_bg_color(obj, background_color, 0);
    lv_obj_set_style_bg_color(obj,
                              lv_color_mix(badge->pressed_color, background_color,
                                           BADGE_PRESSED_MIX),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj,
                                  lv_color_hex(light ? 0x000000 : 0xE1E1E1), 0);
    lv_obj_set_style_border_opa(obj, BADGE_BORDER_OPA, 0);
    lv_obj_set_style_text_color(badge->text_label,
                                lv_color_hex(text_color), 0);
}

void ha_badge_set_body_clickable(lv_obj_t *obj, bool clickable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
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

static void ha_badge_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    ha_badge_t *badge = (ha_badge_t *)obj;
    badge->click_cb = NULL;
    badge->hold_cb = NULL;
    badge->pressed_color = lv_color_hex(0x9E9E9E);
    badge->unavailable = false;

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, BADGE_HEIGHT);
    lv_obj_set_style_max_width(obj, LV_HOR_RES - 40, 0);
    lv_obj_set_style_radius(obj, BADGE_HEIGHT / 2, 0);
    // lv_obj_set_style_clip_corner(obj, true, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(BADGE_DARK_BACKGROUND_COLOR), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
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
    lv_obj_set_style_text_font(badge->text_label, ha_ui_font_regular(14), 0);
    ha_badge_apply_theme(obj);
}

static void ha_badge_event(const lv_obj_class_t *class_p, lv_event_t *event)
{
    LV_UNUSED(class_p);
    if (lv_obj_event_base(MY_CLASS, event) != LV_RESULT_OK) {
        return;
    }

    ha_badge_t *badge = (ha_badge_t *)lv_event_get_target_obj(event);
    switch (lv_event_get_code(event)) {
    /* LV_STATE_PRESSED supplies the flat pressed color; no ripple draw. */
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
    default:
        break;
    }
}

