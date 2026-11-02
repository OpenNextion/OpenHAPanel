// main/common/ha_control_button/ha_control_button.c
#include "ha_control_button.h"
#include "ui_i18n.h"
#include "i18n.h"
#include <stddef.h>

#include "assets/ha_ui_assets.h"

#define BUTTON_HEIGHT              56
#define BUTTON_RADIUS              12
#define BUTTON_BG                  0x242424
#define BUTTON_PRESSED_BG          0x373737
#define BUTTON_TEXT_ENABLED        0xFFFFFF
#define BUTTON_TEXT_DISABLED       0x909090

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

static void apply_text_style(ha_control_button_context_t *ctx)
{
    lv_obj_set_style_text_color(ctx->label,
                                lv_color_hex(ctx->disabled
                                             ? BUTTON_TEXT_DISABLED
                                             : BUTTON_TEXT_ENABLED),
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
    lv_obj_remove_style_all(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(btn, LV_PCT(100), BUTTON_HEIGHT);
    lv_obj_set_style_radius(btn, BUTTON_RADIUS, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(BUTTON_BG), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(BUTTON_PRESSED_BG),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(BUTTON_BG),
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
    lv_obj_set_style_text_font(label, &ha_font_misan_medium_20, 0);
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
