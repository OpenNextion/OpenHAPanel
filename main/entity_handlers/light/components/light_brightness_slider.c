#include "light_brightness_slider.h"
#include "ha_ui_font.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "../../../assets/ha_ui_assets.h"
#include "light_detail_theme.h"

#define BRIGHTNESS_SLIDER_WIDTH          88
#define BRIGHTNESS_SLIDER_HEIGHT         230
#define BRIGHTNESS_SLIDER_RADIUS         20
#define BRIGHTNESS_FILL_TOP_RADIUS       8
#define BRIGHTNESS_HANDLE_INSET          22
#define BRIGHTNESS_HANDLE_Y_OFFSET       11
#define BRIGHTNESS_HANDLE_HEIGHT         4
#define BRIGHTNESS_END_CAP               12
#define BRIGHTNESS_DRAG_THRESHOLD        3
#define BRIGHTNESS_HOLD_DELAY_MS         220
#define BRIGHTNESS_TRANSITION_MS         180
#define BRIGHTNESS_MIN_ACTIVE_PERCENT    1
#define BRIGHTNESS_EXT_DRAW_SIZE          80
#define BRIGHTNESS_OFF_TRACK_COLOR       0x202020
#define BRIGHTNESS_ACTIVE_TRACK_OPACITY  51

typedef struct {
    lv_obj_t *slider;
    lv_obj_t *track;
    lv_obj_t *fill;
    lv_obj_t *handle;
    lv_obj_t *tip;
    lv_obj_t *tip_label;
    uint8_t committed_value;
    int32_t visual_value;
    lv_color_t display_color;
    lv_point_t press_origin;
    uint32_t press_tick;
    int32_t last_drag_value;
    bool has_last_drag_value;
    bool power_on;
    bool pressed;
    bool dragging;
    bool suppress_callback;
    bool disabled;
    light_brightness_change_cb_t callback;
    void *user_data;
} light_brightness_slider_context_t;

static light_brightness_slider_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static int32_t clamp_percent(int32_t value)
{
    return LV_CLAMP(0, value, 100);
}

static int32_t clamp_active_percent(int32_t value)
{
    return LV_CLAMP(BRIGHTNESS_MIN_ACTIVE_PERCENT, value, 100);
}

static uint32_t rgb_value(uint32_t red, uint32_t green, uint32_t blue)
{
    return (red << 16) | (green << 8) | blue;
}

static uint32_t color_value(lv_color_t color)
{
    return lv_color_to_u32(color) & 0x00FFFFFFU;
}

/* Preserve hue while applying the same low-saturation contrast correction as
 * the Floor lamp brightness control. */
static lv_color_t contrast_color(lv_color_t input)
{
    const uint32_t value = color_value(input);
    float channels[] = {
        (float)((value >> 16) & 0xFF),
        (float)((value >> 8) & 0xFF),
        (float)(value & 0xFF),
    };
    const float high = LV_MAX(channels[0],
                              LV_MAX(channels[1], channels[2]));
    const float low = LV_MIN(channels[0],
                             LV_MIN(channels[1], channels[2]));
    const float saturation = high > 0.0f ? (high - low) / high : 0.0f;
    if (saturation < 0.1f) {
        for (size_t i = 0; i < 3; i++) {
            channels[i] = high > 0.0f ? channels[i] * 225.0f / high
                                      : 225.0f;
        }
    }
    else if (saturation < 0.4f) {
        for (size_t i = 0; i < 3; i++) {
            channels[i] = high -
                          (high - channels[i]) * 0.4f / saturation;
        }
    }
    return lv_color_hex(rgb_value(
        (uint32_t)LV_CLAMP(0, (int32_t)lroundf(channels[0]), 255),
        (uint32_t)LV_CLAMP(0, (int32_t)lroundf(channels[1]), 255),
        (uint32_t)LV_CLAMP(0, (int32_t)lroundf(channels[2]), 255)));
}

static lv_color_t active_track_color(lv_color_t color)
{
    const uint32_t value = color_value(color);
    const uint32_t red = ((value >> 16) & 0xFF) *
                         BRIGHTNESS_ACTIVE_TRACK_OPACITY / 255;
    const uint32_t green = ((value >> 8) & 0xFF) *
                           BRIGHTNESS_ACTIVE_TRACK_OPACITY / 255;
    const uint32_t blue = (value & 0xFF) *
                          BRIGHTNESS_ACTIVE_TRACK_OPACITY / 255;
    return lv_color_hex(rgb_value(red, green, blue));
}

static int32_t fill_top(const lv_area_t *area, int32_t value)
{
    if (value <= 0) {
        return area->y2 + 1;
    }
    const int32_t normalized = LV_CLAMP(0, value - 1, 99);
    const int32_t travel = lv_area_get_height(area) -
                           BRIGHTNESS_END_CAP * 2 - 1;
    return area->y1 + (99 - normalized) * travel / 99;
}

/* Apply the current state to the track/fill/handle object tree. This mirrors
 * the Cover/Fan sliders: an ordinary track object clips its children to its
 * rounded corners natively, avoiding the previous full-area ARGB8888 layer. */
static void update_visual(light_brightness_slider_context_t *context)
{
    if (!context || !context->slider) {
        return;
    }
    const int32_t width = lv_obj_get_width(context->slider);
    const int32_t height = lv_obj_get_height(context->slider);
    lv_obj_set_size(context->track, width, height);
    const bool light = light_detail_is_light_theme();
    const lv_opa_t opacity = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    const bool effective_on = context->dragging
                                  ? context->visual_value > 0
                                  : context->power_on;
    const lv_color_t fill_color = context->display_color;
    const lv_color_t track_color =
        light
            ? (effective_on
                   ? fill_color
                   : lv_color_hex(LIGHT_DETAIL_LIGHT_INACTIVE))
            : (effective_on
                   ? active_track_color(fill_color)
                   : lv_color_hex(BRIGHTNESS_OFF_TRACK_COLOR));
    lv_obj_set_style_bg_color(context->track, track_color, 0);
    lv_obj_set_style_bg_opa(context->track,
                            light ? (context->disabled ? LV_OPA_10
                                                       : LV_OPA_20)
                                  : opacity,
                            0);

    if (!effective_on || context->visual_value <= 0) {
        lv_obj_add_flag(context->fill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(context->handle, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_area_t local_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = width - 1,
        .y2 = height - 1,
    };
    const int32_t y = fill_top(&local_area, context->visual_value);

    lv_obj_set_style_bg_color(context->fill, fill_color, 0);
    lv_obj_set_style_bg_opa(context->fill, opacity, 0);
    lv_obj_set_pos(context->fill, 0, y);
    /* Keep the fill's lower small-radius corners below the track; the track's
     * corner clip supplies its larger bottom radius. */
    lv_obj_set_size(context->fill,
                    width,
                    height - y + BRIGHTNESS_SLIDER_RADIUS);
    lv_obj_remove_flag(context->fill, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_bg_opa(context->handle, opacity, 0);
    lv_obj_set_pos(context->handle,
                   BRIGHTNESS_HANDLE_INSET,
                   y + BRIGHTNESS_HANDLE_Y_OFFSET);
    lv_obj_set_size(context->handle,
                    width - BRIGHTNESS_HANDLE_INSET * 2,
                    BRIGHTNESS_HANDLE_HEIGHT);
    lv_obj_remove_flag(context->handle, LV_OBJ_FLAG_HIDDEN);
}

static void update_tip(light_brightness_slider_context_t *context)
{
    if (!context->tip || !context->tip_label) {
        return;
    }
    if (!context->dragging || context->disabled) {
        lv_obj_add_flag(context->tip, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char text[16];
    snprintf(text, sizeof(text), "%ld%%", (long)context->visual_value);
    lv_label_set_text(context->tip_label, text);
    lv_obj_remove_flag(context->tip, LV_OBJ_FLAG_HIDDEN);

    const int32_t width = lv_obj_get_width(context->slider);
    const int32_t height = lv_obj_get_height(context->slider);
    lv_area_t local_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = width - 1,
        .y2 = height - 1,
    };
    int32_t y = fill_top(&local_area, context->visual_value) +
                BRIGHTNESS_HANDLE_Y_OFFSET - 18;
    y = LV_CLAMP(0, y, LV_MAX(0, height - 36));
    lv_obj_set_pos(context->tip, -76, y);
    lv_obj_move_foreground(context->tip);
}

static void set_native_value(light_brightness_slider_context_t *context,
                             int32_t value)
{
    context->suppress_callback = true;
    lv_slider_set_value(context->slider, value, LV_ANIM_OFF);
    context->suppress_callback = false;
}

static void set_visual_value(light_brightness_slider_context_t *context,
                             int32_t value)
{
    context->visual_value = clamp_percent(value);
    set_native_value(context, context->visual_value);
    update_visual(context);
    update_tip(context);
}

static int32_t point_value(light_brightness_slider_context_t *context,
                           const lv_point_t *point)
{
    lv_area_t area;
    lv_obj_get_coords(context->slider, &area);
    const int32_t travel = lv_area_get_height(&area) -
                           BRIGHTNESS_END_CAP * 2 - 1;
    if (travel <= 0) {
        return context->visual_value;
    }
    const float position =
        (float)(point->y - area.y1 - BRIGHTNESS_END_CAP);
    const int32_t value =
        100 - (int32_t)lroundf(position * 99.0f / travel);
    return clamp_active_percent(value);
}

static int32_t pointer_value(light_brightness_slider_context_t *context)
{
    lv_indev_t *input = lv_indev_active();
    if (!input) {
        return context->visual_value;
    }
    lv_point_t point;
    lv_indev_get_point(input, &point);
    return point_value(context, &point);
}

static void apply_pointer(light_brightness_slider_context_t *context,
                          bool preview)
{
    const int32_t value = pointer_value(context);
    const bool changed = value != context->visual_value;
    set_visual_value(context, value);
    if (preview && changed && context->callback) {
        context->callback((uint8_t)value,
                          LIGHT_CONTROL_PREVIEW,
                          context->user_data);
    }
}

static void visual_anim_cb(void *var, int32_t value)
{
    light_brightness_slider_context_t *context = var;
    if (!context || !context->slider) {
        return;
    }
    context->visual_value = value;
    update_visual(context);
}

static void animate_visual_to(light_brightness_slider_context_t *context,
                              int32_t value)
{
    lv_anim_delete(context, visual_anim_cb);
    if (context->visual_value == value) {
        update_visual(context);
        return;
    }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, context);
    lv_anim_set_exec_cb(&animation, visual_anim_cb);
    lv_anim_set_values(&animation, context->visual_value, value);
    lv_anim_set_duration(&animation, BRIGHTNESS_TRANSITION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}

static void event_cb(lv_event_t *event)
{
    light_brightness_slider_context_t *context =
        lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(event, BRIGHTNESS_EXT_DRAW_SIZE);
        return;
    }
    if (code == LV_EVENT_DELETE) {
        lv_anim_delete(context, visual_anim_cb);
        context->slider = NULL;
        lv_free(context);
        return;
    }
    if (code == LV_EVENT_SIZE_CHANGED) {
        update_visual(context);
        return;
    }
    if (context->disabled || context->suppress_callback) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        context->pressed = true;
        context->dragging = false;
        context->has_last_drag_value = false;
        context->press_tick = lv_tick_get();
        lv_indev_t *input = lv_indev_active();
        if (input) {
            lv_indev_get_point(input, &context->press_origin);
        }
        return;
    }
    if (code == LV_EVENT_PRESSING && context->pressed) {
        lv_indev_t *input = lv_indev_active();
        if (!input) {
            return;
        }
        lv_point_t point;
        lv_indev_get_point(input, &point);
        if (!context->dragging &&
            (LV_ABS(point.x - context->press_origin.x) +
                     LV_ABS(point.y - context->press_origin.y) >
                 BRIGHTNESS_DRAG_THRESHOLD ||
             lv_tick_elaps(context->press_tick) >=
                 BRIGHTNESS_HOLD_DELAY_MS)) {
            context->dragging = true;
        }
        if (context->dragging) {
            apply_pointer(context, true);
            /* Cache the last stable sample while the finger is still touching
             * the glass. The release event samples one final point as the
             * finger lifts, where capacitive coordinates can drift, so the
             * commit must reuse this cached value instead. */
            context->last_drag_value = context->visual_value;
            context->has_last_drag_value = true;
        }
        return;
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_indev_t *input = lv_indev_active();
        if (!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) {
            const int32_t value = clamp_active_percent(
                lv_slider_get_value(context->slider));
            set_native_value(context, value);
            context->visual_value = value;
            update_visual(context);
            if (context->callback) {
                context->callback((uint8_t)context->visual_value,
                                  LIGHT_CONTROL_PREVIEW,
                                  context->user_data);
            }
        }
        return;
    }
    if (code == LV_EVENT_RELEASED && context->pressed) {
        const bool was_dragging = context->dragging;
        int32_t value;
        if (was_dragging && context->has_last_drag_value) {
            value = context->last_drag_value;
        }
        else {
            value = point_value(context, &context->press_origin);
        }
        if (was_dragging) {
            set_visual_value(context, value);
        }
        else {
            set_native_value(context, value);
        }
        context->pressed = false;
        context->dragging = false;
        context->has_last_drag_value = false;
        context->committed_value = (uint8_t)value;
        context->power_on = context->committed_value > 0;
        update_tip(context);
        if (was_dragging) {
            update_visual(context);
        }
        else {
            animate_visual_to(context, value);
        }
        if (context->callback) {
            context->callback(context->committed_value,
                              LIGHT_CONTROL_COMMIT,
                              context->user_data);
        }
        return;
    }
    if (code == LV_EVENT_PRESS_LOST && context->pressed) {
        context->pressed = false;
        context->dragging = false;
        context->has_last_drag_value = false;
        set_visual_value(context, context->committed_value);
        update_tip(context);
        if (context->callback) {
            context->callback(context->committed_value,
                              LIGHT_CONTROL_CANCEL,
                              context->user_data);
        }
    }
}

/* The native slider recomputes its value from the release coordinate before
 * regular event callbacks run. Own pointer input in the preprocess phase so
 * that a drifting release sample cannot replace the displayed value. */
static void pointer_event_cb(lv_event_t *event)
{
    lv_indev_t *input = lv_indev_active();
    if (!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) {
        return;
    }
    event_cb(event);
    lv_event_stop_processing(event);
}

lv_obj_t *light_brightness_slider_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_brightness_slider_context_t *context =
        lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }
    /* The caller supplies the light's resolved color. Until then, avoid a
     * yellow flash when an off light is dragged before HA reports its color. */
    context->display_color = contrast_color(lv_color_hex(0xE1E1E1));

    lv_obj_t *slider = lv_slider_create(parent);
    if (!slider) {
        lv_free(context);
        return NULL;
    }
    context->slider = slider;
    lv_obj_set_user_data(slider, context);
    lv_obj_set_size(slider,
                    BRIGHTNESS_SLIDER_WIDTH,
                    BRIGHTNESS_SLIDER_HEIGHT);
    lv_slider_set_orientation(slider, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 0, LV_ANIM_OFF);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);

    context->track = lv_obj_create(slider);
    lv_obj_remove_style_all(context->track);
    lv_obj_remove_flag(context->track,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(context->track, 0, 0);
    lv_obj_set_size(context->track,
                    BRIGHTNESS_SLIDER_WIDTH,
                    BRIGHTNESS_SLIDER_HEIGHT);
    lv_obj_set_style_radius(context->track, BRIGHTNESS_SLIDER_RADIUS, 0);
    lv_obj_set_style_clip_corner(context->track, true, 0);
    lv_obj_set_style_border_width(context->track, 0, 0);
    lv_obj_set_style_pad_all(context->track, 0, 0);

    context->fill = lv_obj_create(context->track);
    lv_obj_remove_style_all(context->fill);
    lv_obj_remove_flag(context->fill,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(context->fill, BRIGHTNESS_FILL_TOP_RADIUS, 0);
    lv_obj_set_style_border_width(context->fill, 0, 0);
    lv_obj_set_style_pad_all(context->fill, 0, 0);
    lv_obj_add_flag(context->fill, LV_OBJ_FLAG_HIDDEN);

    context->handle = lv_obj_create(context->track);
    lv_obj_remove_style_all(context->handle);
    lv_obj_remove_flag(context->handle,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(context->handle,
                            BRIGHTNESS_HANDLE_HEIGHT / 2,
                            0);
    lv_obj_set_style_bg_color(context->handle, lv_color_white(), 0);
    lv_obj_set_style_border_width(context->handle, 0, 0);
    lv_obj_set_style_pad_all(context->handle, 0, 0);
    lv_obj_add_flag(context->handle, LV_OBJ_FLAG_HIDDEN);

    context->tip = lv_obj_create(slider);
    if (context->tip) {
        lv_obj_remove_flag(context->tip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(context->tip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(context->tip, 64, 36);
        lv_obj_set_style_radius(context->tip, 8, 0);
        lv_obj_set_style_border_width(context->tip, 0, 0);
        lv_obj_set_style_shadow_width(context->tip, 0, 0);
        lv_obj_set_style_bg_color(context->tip, lv_color_hex(0x303030), 0);
        lv_obj_set_style_bg_opa(context->tip, LV_OPA_COVER, 0);
        context->tip_label = lv_label_create(context->tip);
        if (context->tip_label) {
            lv_obj_set_style_text_font(context->tip_label, ha_ui_font_regular(14), 0);
            lv_obj_set_style_text_color(context->tip_label,
                                        lv_color_white(),
                                        0);
            lv_obj_center(context->tip_label);
        }
        lv_obj_add_flag(context->tip, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_ALL, context);
    lv_obj_add_event_cb(slider, pointer_event_cb,
                        LV_EVENT_PRESSED | LV_EVENT_PREPROCESS, context);
    lv_obj_add_event_cb(slider, pointer_event_cb,
                        LV_EVENT_PRESSING | LV_EVENT_PREPROCESS, context);
    lv_obj_add_event_cb(slider, pointer_event_cb,
                        LV_EVENT_RELEASED | LV_EVENT_PREPROCESS, context);
    lv_obj_add_event_cb(slider, pointer_event_cb,
                        LV_EVENT_PRESS_LOST | LV_EVENT_PREPROCESS, context);
    lv_obj_refresh_ext_draw_size(slider);
    update_visual(context);
    return slider;
}

void light_brightness_slider_set_value(lv_obj_t *obj,
                                       uint8_t percent,
                                       lv_anim_enable_t animate)
{
    light_brightness_slider_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    percent = (uint8_t)clamp_percent(percent);
    context->committed_value = percent;
    set_native_value(context, percent);
    lv_anim_delete(context, visual_anim_cb);
    if (animate == LV_ANIM_ON && !context->dragging &&
        context->visual_value != percent) {
        animate_visual_to(context, percent);
    }
    else {
        context->visual_value = percent;
        update_visual(context);
    }
}

void light_brightness_slider_set_active_color(lv_obj_t *obj, lv_color_t color)
{
    light_brightness_slider_context_t *context = get_context(obj);
    if (context) {
        context->display_color = contrast_color(color);
        update_visual(context);
    }
}

void light_brightness_slider_set_power(lv_obj_t *obj, bool on)
{
    light_brightness_slider_context_t *context = get_context(obj);
    if (context) {
        context->power_on = on;
        if (!context->dragging) {
            lv_anim_delete(context, visual_anim_cb);
            context->visual_value = on ? context->committed_value : 0;
            set_native_value(context, context->visual_value);
        }
        update_visual(context);
    }
}

void light_brightness_slider_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_brightness_slider_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->disabled = disabled;
    context->pressed = false;
    context->dragging = false;
    if (disabled) {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
    else {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    }
    if (context->tip) {
        lv_obj_add_flag(context->tip, LV_OBJ_FLAG_HIDDEN);
    }
    update_visual(context);
}

void light_brightness_slider_set_change_cb(
    lv_obj_t *obj,
    light_brightness_change_cb_t callback,
    void *user_data)
{
    light_brightness_slider_context_t *context = get_context(obj);
    if (context) {
        context->callback = callback;
        context->user_data = user_data;
    }
}
