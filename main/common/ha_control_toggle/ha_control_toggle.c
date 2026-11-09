#include "ha_control_toggle.h"

#include <stddef.h>

#include "assets/ha_ui_assets.h"
#include "screen_theme.h"

#define TOGGLE_HEIGHT          56
#define TOGGLE_TRACK_RADIUS    20
#define TOGGLE_KNOB_RADIUS     20

#define TOGGLE_TRACK_OPA        51
#define TOGGLE_TRACK_OFF       0x9E9E9E
#define TOGGLE_KNOB_OFF        0x9E9E9E
#define TOGGLE_DISABLED_TRACK  0x101112
#define TOGGLE_DISABLED_KNOB   0x454545

#define TOGGLE_TRACK_ON_LIGHT      0xFFF3CD
#define TOGGLE_KNOB_ON_LIGHT       0xFFC107
#if LV_COLOR_DEPTH < 24
#define TOGGLE_TRACK_OFF_LIGHT     0xE3E3E3
#else
#define TOGGLE_TRACK_OFF_LIGHT     0xECECEC
#endif
#define TOGGLE_KNOB_OFF_LIGHT      0x9E9E9E
#define TOGGLE_DISABLED_TRACK_LIGHT 0xE2E4E8
#define TOGGLE_DISABLED_KNOB_LIGHT 0xDEDEDE

typedef struct {
    lv_obj_t *track;
    lv_obj_t *knob;
    lv_obj_t *icon_label;
    bool on;
    bool disabled;
    lv_color_t active_color;
    lv_color_t track_off_light;
    lv_color_t track_on_light;
    lv_color_t knob_off_light;
    lv_color_t knob_on_light;
    lv_color_t disabled_track_light;
    lv_color_t disabled_knob_light;
    ha_control_toggle_change_cb_t callback;
    void *user_data;
} ha_control_toggle_context_t;

static ha_control_toggle_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static int32_t knob_x_off(const lv_obj_t *track)
{
    return 0;
}

static int32_t knob_x_on(const lv_obj_t *track)
{
    return lv_obj_get_width(track) / 2;
}

static void knob_x_anim_cb(void *var, int32_t value)
{
    lv_obj_t *knob = var;
    lv_obj_set_x(knob, value);
}

static void animate_knob_x(lv_obj_t *knob, int32_t from, int32_t to)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, knob);
    lv_anim_set_exec_cb(&a, knob_x_anim_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void apply_track_style(ha_control_toggle_context_t *ctx)
{
    lv_obj_t *track = ctx->track;
    const bool light = (screen_theme_get() == SCREEN_THEME_LIGHT);

    if (ctx->disabled) {
        lv_obj_set_style_bg_color(track,
            light ? ctx->disabled_track_light
                  : lv_color_hex(TOGGLE_DISABLED_TRACK),
            0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        return;
    }
    if (ctx->on) {
        lv_obj_set_style_bg_color(track,
            light ? ctx->track_on_light : ctx->active_color,
            0);
        lv_obj_set_style_bg_opa(track, light ? LV_OPA_COVER : TOGGLE_TRACK_OPA,
                                0);
    } else {
        lv_obj_set_style_bg_color(track,
            light ? ctx->track_off_light : lv_color_hex(TOGGLE_TRACK_OFF),
            0);
        lv_obj_set_style_bg_opa(track, light ? LV_OPA_COVER : TOGGLE_TRACK_OPA,
                                0);
    }
}

static void apply_knob_style(ha_control_toggle_context_t *ctx)
{
    lv_obj_t *knob = ctx->knob;
    const bool light = (screen_theme_get() == SCREEN_THEME_LIGHT);

    if (ctx->disabled) {
        lv_obj_set_style_bg_color(knob,
            light ? ctx->disabled_knob_light
                  : lv_color_hex(TOGGLE_DISABLED_KNOB),
            0);
        return;
    }
    lv_obj_set_style_bg_color(knob,
        ctx->on ? (light ? ctx->knob_on_light : ctx->active_color)
                : (light ? ctx->knob_off_light
                         : lv_color_hex(TOGGLE_KNOB_OFF)),
        0);
}

static void event_cb(lv_event_t *event)
{
    ha_control_toggle_context_t *ctx = lv_event_get_user_data(event);
    if (!ctx) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_DELETE) {
        ctx->track = NULL;
        ctx->knob = NULL;
        lv_free(ctx);
        return;
    }
    if (code == LV_EVENT_SIZE_CHANGED) {
        lv_obj_update_layout(ctx->track);
        const int32_t target = ctx->on ? knob_x_on(ctx->track)
                                       : knob_x_off(ctx->track);
        lv_obj_set_x(ctx->knob, target);
        return;
    }
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING ||
        code == LV_EVENT_RELEASED || code == LV_EVENT_CLICKED) {
        lv_event_stop_bubbling(event);
    }
    if (ctx->disabled || code != LV_EVENT_CLICKED) {
        return;
    }
    ctx->on = !ctx->on;
    apply_track_style(ctx);
    apply_knob_style(ctx);
    const int32_t target = ctx->on ? knob_x_on(ctx->track)
                                   : knob_x_off(ctx->track);
    animate_knob_x(ctx->knob, lv_obj_get_x(ctx->knob), target);
    if (ctx->callback) {
        ctx->callback(ctx->on, ctx->user_data);
    }
}

lv_obj_t *ha_control_toggle_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    ha_control_toggle_context_t *ctx = lv_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    ctx->active_color = lv_color_hex(0xFFC107);
    ctx->track_off_light = lv_color_hex(TOGGLE_TRACK_OFF_LIGHT);
    ctx->track_on_light = lv_color_hex(TOGGLE_TRACK_ON_LIGHT);
    ctx->knob_off_light = lv_color_hex(TOGGLE_KNOB_OFF_LIGHT);
    ctx->knob_on_light = lv_color_hex(TOGGLE_KNOB_ON_LIGHT);
    ctx->disabled_track_light = lv_color_hex(TOGGLE_DISABLED_TRACK_LIGHT);
    ctx->disabled_knob_light = lv_color_hex(TOGGLE_DISABLED_KNOB_LIGHT);

    lv_obj_t *track = lv_obj_create(parent);
    if (!track) {
        lv_free(ctx);
        return NULL;
    }
    ctx->track = track;
    lv_obj_remove_style_all(track);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(track, LV_PCT(100), TOGGLE_HEIGHT);
    lv_obj_set_style_radius(track, TOGGLE_TRACK_RADIUS, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    apply_track_style(ctx);
    lv_obj_set_user_data(track, ctx);
    lv_obj_add_event_cb(track, event_cb, LV_EVENT_ALL, ctx);

    lv_obj_t *knob = lv_obj_create(track);
    ctx->knob = knob;
    lv_obj_remove_style_all(knob);
    lv_obj_remove_flag(knob, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(knob, LV_PCT(50), TOGGLE_HEIGHT);
    lv_obj_set_style_radius(knob, TOGGLE_KNOB_RADIUS, 0);
    lv_obj_set_style_border_width(knob, 0, 0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    apply_knob_style(ctx);
    lv_obj_set_pos(knob, 0, 0);

    lv_obj_t *label = lv_label_create(knob);
    lv_obj_set_style_text_font(label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);
    ctx->icon_label = label;

    return track;
}

void ha_control_toggle_set_value(lv_obj_t *obj, bool on,
                                 lv_anim_enable_t animate)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    if (!ctx || !ctx->track || !ctx->knob) {
        return;
    }
    ctx->on = on;
    apply_track_style(ctx);
    apply_knob_style(ctx);
    lv_obj_update_layout(ctx->track);
    const int32_t target = on ? knob_x_on(ctx->track) : knob_x_off(ctx->track);
    if (animate) {
        animate_knob_x(ctx->knob, lv_obj_get_x(ctx->knob), target);
    } else {
        lv_obj_set_x(ctx->knob, target);
    }
}

void ha_control_toggle_set_active_color(lv_obj_t *obj, lv_color_t color)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    if (ctx) {
        ctx->active_color = color;
        if (ctx->on) {
            apply_track_style(ctx);
            apply_knob_style(ctx);
        }
    }
}

void ha_control_toggle_set_disabled(lv_obj_t *obj, bool disabled)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    if (!ctx || !ctx->track) {
        return;
    }
    ctx->disabled = disabled;
    if (disabled) {
        lv_obj_add_state(ctx->track, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(ctx->track, LV_STATE_DISABLED);
    }
    apply_track_style(ctx);
    apply_knob_style(ctx);
}

void ha_control_toggle_set_change_cb(lv_obj_t *obj,
                                     ha_control_toggle_change_cb_t callback,
                                     void *user_data)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    if (ctx) {
        ctx->callback = callback;
        ctx->user_data = user_data;
    }
}

lv_obj_t *ha_control_toggle_get_knob(lv_obj_t *obj)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    return ctx ? ctx->knob : NULL;
}

void ha_control_toggle_set_icon_text(lv_obj_t *obj, const char *text)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    if (ctx && ctx->icon_label) {
        lv_label_set_text(ctx->icon_label, text ? text : "");
    }
}

void ha_control_toggle_refresh_theme(lv_obj_t *obj)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    if (!ctx) {
        return;
    }
    apply_track_style(ctx);
    apply_knob_style(ctx);
}

void ha_control_toggle_set_light_colors(lv_obj_t *obj,
                                        lv_color_t track_off,
                                        lv_color_t track_on,
                                        lv_color_t knob_off,
                                        lv_color_t knob_on,
                                        lv_color_t disabled_track,
                                        lv_color_t disabled_knob)
{
    ha_control_toggle_context_t *ctx = get_context(obj);
    if (!ctx) {
        return;
    }
    ctx->track_off_light = track_off;
    ctx->track_on_light = track_on;
    ctx->knob_off_light = knob_off;
    ctx->knob_on_light = knob_on;
    ctx->disabled_track_light = disabled_track;
    ctx->disabled_knob_light = disabled_knob;
    ha_control_toggle_refresh_theme(obj);
}
