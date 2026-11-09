// main/common/ha_control_button/ha_control_button.c
#include "ha_control_button.h"
#include "ha_ui_font.h"
#include "ui_i18n.h"
#include "i18n.h"
#include "screen_theme.h"
#include <stddef.h>

#define BUTTON_HEIGHT              56
#define BUTTON_RADIUS              12

#define BUTTON_TEXT_ENABLED_DARK   0xE1E1E1
#define BUTTON_TEXT_DISABLED_DARK  0x909090

/* RGB565 needs stronger separation from the white tile surface. */
#if LV_COLOR_DEPTH < 24
#define BUTTON_BG_DARK             0x333333
#define BUTTON_PRESSED_BG_DARK     0x4F4F4F
#define BUTTON_BG_LIGHT            0xE3E3E3
#define BUTTON_PRESSED_BG_LIGHT    0xD1D1D1
#else
#define BUTTON_BG_DARK             0x242424
#define BUTTON_PRESSED_BG_DARK     0x333333
#define BUTTON_BG_LIGHT            0xF2F2F2
#define BUTTON_PRESSED_BG_LIGHT    0xE0E0E0
#endif
#define BUTTON_TEXT_ENABLED_LIGHT  0x141414
#define BUTTON_TEXT_DISABLED_LIGHT 0xBDBDBD

typedef struct {
    lv_obj_t *label;
    bool disabled;
    ha_control_button_press_cb_t callback;
    void *user_data;
} ha_control_button_context_t;

static ha_control_button_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static uint32_t button_text_color(bool disabled)
{
    const bool light = (screen_theme_get() == SCREEN_THEME_LIGHT);
    if (disabled) {
        return light ? BUTTON_TEXT_DISABLED_LIGHT : BUTTON_TEXT_DISABLED_DARK;
    }
    return light ? BUTTON_TEXT_ENABLED_LIGHT : BUTTON_TEXT_ENABLED_DARK;
}

static void apply_text_style(ha_control_button_context_t *ctx)
{
    lv_obj_set_style_text_color(ctx->label,
                                lv_color_hex(button_text_color(ctx->disabled)),
                                0);
}

static void event_cb(lv_event_t *event)
{
    ha_control_button_context_t *ctx = lv_event_get_user_data(event);
    if (!ctx) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_DELETE) {
        lv_free(ctx);
        return;
    }
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING ||
        code == LV_EVENT_RELEASED || code == LV_EVENT_CLICKED) {
        lv_event_stop_bubbling(event);
    }
    if (ctx->disabled || code != LV_EVENT_CLICKED) {
        return;
    }
    if (ctx->callback) {
        ctx->callback(ctx->user_data);
    }
}

lv_obj_t *ha_control_button_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    ha_control_button_context_t *ctx = lv_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    lv_obj_t *btn = lv_button_create(parent);
    if (!btn) {
        lv_free(ctx);
        return NULL;
    }
    const bool light = (screen_theme_get() == SCREEN_THEME_LIGHT);
    const uint32_t bg_color = light ? BUTTON_BG_LIGHT : BUTTON_BG_DARK;
    const uint32_t pressed_bg = light ? BUTTON_PRESSED_BG_LIGHT
                                      : BUTTON_PRESSED_BG_DARK;

    lv_obj_remove_style_all(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(btn, LV_PCT(100), BUTTON_HEIGHT);
    lv_obj_set_style_radius(btn, BUTTON_RADIUS, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_bg),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_color),
                              LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_transform_width(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_x(btn, 256, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn, 256, LV_STATE_PRESSED);
    lv_obj_set_user_data(btn, ctx);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, ctx);

    lv_obj_t *label = lv_label_create(btn);
    ctx->label = label;
    ui_i18n_label_set_text(label, i18n_get(I18N_ACTION_PRESS));
    lv_obj_set_style_text_font(label, ha_ui_font_medium(20), 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    apply_text_style(ctx);
    lv_obj_center(label);

    return btn;
}

void ha_control_button_set_disabled(lv_obj_t *obj, bool disabled)
{
    ha_control_button_context_t *ctx = get_context(obj);
    if (!ctx) {
        return;
    }
    ctx->disabled = disabled;
    if (disabled) {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    }
    apply_text_style(ctx);
}

void ha_control_button_set_press_cb(lv_obj_t *obj,
                                    ha_control_button_press_cb_t callback,
                                    void *user_data)
{
    ha_control_button_context_t *ctx = get_context(obj);
    if (ctx) {
        ctx->callback = callback;
        ctx->user_data = user_data;
    }
}

void ha_control_button_refresh_theme(lv_obj_t *obj)
{
    ha_control_button_context_t *ctx = get_context(obj);
    if (!ctx) {
        return;
    }
    const bool light = (screen_theme_get() == SCREEN_THEME_LIGHT);
    const uint32_t bg_color = light ? BUTTON_BG_LIGHT : BUTTON_BG_DARK;
    const uint32_t pressed_bg = light ? BUTTON_PRESSED_BG_LIGHT
                                      : BUTTON_PRESSED_BG_DARK;

    lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(pressed_bg),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color),
                              LV_STATE_DISABLED);
    apply_text_style(ctx);
}
