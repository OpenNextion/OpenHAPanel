#include "light_brightness_slider.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

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
    lv_obj_t *tip;
    lv_obj_t *tip_label;
    uint8_t committed_value;
    int32_t visual_value;
    lv_color_t display_color;
    lv_point_t press_origin;
    uint32_t press_tick;
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

static void draw_rect(lv_layer_t *layer,
                      const lv_area_t *area,
                      lv_color_t color,
                      lv_opa_t opacity,
                      int32_t radius)
{
    if (area->x2 < area->x1 || area->y2 < area->y1) {
        return;
    }
    const lv_area_t *clip = &layer->_clip_area;
    if (area->x1 > clip->x2 || area->x2 < clip->x1 ||
        area->y1 > clip->y2 || area->y2 < clip->y1) {
        return;
    }
    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = color;
    descriptor.bg_opa = opacity;
    descriptor.radius = radius;
    lv_draw_rect(layer, &descriptor, area);
}

static void finish_draw_batch(lv_layer_t *layer)
{
    while (layer->draw_task_head) {
        lv_draw_dispatch_wait_for_request();
        lv_draw_dispatch();
    }
}

static int32_t rounded_inset(const lv_area_t *area,
                             int32_t radius,
                             int32_t y)
{
    const int32_t delta =
        y < area->y1 + radius
            ? area->y1 + radius - y - 1
            : y > area->y2 - radius
                  ? y - (area->y2 - radius) - 1
                  : -1;
    return delta >= 0
               ? radius -
                     (int32_t)sqrtf((float)(radius * radius - delta * delta))
               : 0;
}

typedef void (*rounded_draw_cb_t)(lv_layer_t *layer,
                                  const lv_area_t *area,
                                  void *user_data);

static void draw_with_rounded_clip(lv_layer_t *layer,
                                   const lv_area_t *area,
                                   int32_t radius,
                                   rounded_draw_cb_t callback,
                                   void *user_data)
{
    const lv_area_t saved_clip = layer->_clip_area;
    const lv_area_t clip = {
        .x1 = LV_MAX(saved_clip.x1, area->x1),
        .y1 = LV_MAX(saved_clip.y1, area->y1),
        .x2 = LV_MIN(saved_clip.x2, area->x2),
        .y2 = LV_MIN(saved_clip.y2, area->y2),
    };
    if (clip.x1 > clip.x2 || clip.y1 > clip.y2) {
        return;
    }
    radius = LV_MIN(radius,
                    LV_MIN(lv_area_get_width(area),
                           lv_area_get_height(area)) /
                        2);
    unsigned batch_callbacks = 0;
    for (int32_t y = clip.y1; y <= clip.y2;) {
        const int32_t inset = rounded_inset(area, radius, y);
        int32_t end = y;
        while (end < clip.y2 &&
               rounded_inset(area, radius, end + 1) == inset) {
            end++;
        }
        layer->_clip_area = (lv_area_t) {
            .x1 = LV_MAX(clip.x1, area->x1 + inset),
            .y1 = y,
            .x2 = LV_MIN(clip.x2, area->x2 - inset),
            .y2 = end,
        };
        if (layer->_clip_area.x1 <= layer->_clip_area.x2) {
            callback(layer, area, user_data);
            /* A brightness callback creates at most three draw tasks. Drain
             * every four clipped bands without inspecting LVGL's private
             * lv_draw_task_t linked-list fields. */
            if (++batch_callbacks == 4U) {
                finish_draw_batch(layer);
                batch_callbacks = 0;
            }
        }
        y = end + 1;
    }
    layer->_clip_area = saved_clip;
}

static void draw_slider(lv_layer_t *layer,
                        const lv_area_t *area,
                        void *user_data)
{
    light_brightness_slider_context_t *context = user_data;
    const lv_opa_t opacity = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    const bool effective_on = context->dragging
                                  ? context->visual_value > 0
                                  : context->power_on;
    const lv_color_t fill_color = context->display_color;
    const lv_color_t track_color = effective_on
                                       ? active_track_color(fill_color)
                                       : lv_color_hex(
                                             BRIGHTNESS_OFF_TRACK_COLOR);
    draw_rect(layer,
              area,
              track_color,
              opacity,
              BRIGHTNESS_SLIDER_RADIUS);
    if (!effective_on || context->visual_value <= 0) {
        return;
    }

    const int32_t y = fill_top(area, context->visual_value);
    lv_area_t fill = {
        .x1 = area->x1,
        .y1 = y,
        .x2 = area->x2,
        /* Keep the fill's lower small-radius corners below the track. The
         * outer clip supplies the track's larger bottom radius. */
        .y2 = area->y2 + BRIGHTNESS_SLIDER_RADIUS,
    };
    draw_rect(layer,
              &fill,
              fill_color,
              opacity,
              BRIGHTNESS_FILL_TOP_RADIUS);

    lv_area_t handle = {
        .x1 = area->x1 + BRIGHTNESS_HANDLE_INSET,
        .y1 = y + BRIGHTNESS_HANDLE_Y_OFFSET,
        .x2 = area->x2 - BRIGHTNESS_HANDLE_INSET,
        .y2 = y + BRIGHTNESS_HANDLE_Y_OFFSET +
              BRIGHTNESS_HANDLE_HEIGHT - 1,
    };
    draw_rect(layer,
              &handle,
              lv_color_white(),
              opacity,
              BRIGHTNESS_HANDLE_HEIGHT / 2);
}

static void draw_cb(lv_event_t *event)
{
    light_brightness_slider_context_t *context =
        lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    lv_area_t area;
    lv_obj_get_coords(context->slider, &area);
    draw_with_rounded_clip(lv_event_get_layer(event),
                           &area,
                           BRIGHTNESS_SLIDER_RADIUS,
                           draw_slider,
                           context);
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

    lv_area_t local_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = BRIGHTNESS_SLIDER_WIDTH - 1,
        .y2 = BRIGHTNESS_SLIDER_HEIGHT - 1,
    };
    int32_t y = fill_top(&local_area, context->visual_value) +
                BRIGHTNESS_HANDLE_Y_OFFSET - 18;
    y = LV_CLAMP(0, y, BRIGHTNESS_SLIDER_HEIGHT - 36);
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
    lv_obj_invalidate(context->slider);
    update_tip(context);
}

static int32_t pointer_value(light_brightness_slider_context_t *context)
{
    lv_indev_t *input = lv_indev_active();
    if (!input) {
        return context->visual_value;
    }
    lv_point_t point;
    lv_indev_get_point(input, &point);
    lv_area_t area;
    lv_obj_get_coords(context->slider, &area);
    const int32_t travel = lv_area_get_height(&area) -
                           BRIGHTNESS_END_CAP * 2 - 1;
    if (travel <= 0) {
        return context->visual_value;
    }
    const float position =
        (float)(point.y - area.y1 - BRIGHTNESS_END_CAP);
    const int32_t value =
        100 - (int32_t)lroundf(position * 99.0f / travel);
    return clamp_active_percent(value);
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
    lv_obj_invalidate(context->slider);
}

static void animate_visual_to(light_brightness_slider_context_t *context,
                              int32_t value)
{
    lv_anim_delete(context, visual_anim_cb);
    if (context->visual_value == value) {
        lv_obj_invalidate(context->slider);
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
    if (code == LV_EVENT_DRAW_MAIN) {
        draw_cb(event);
        return;
    }
    if (context->disabled || context->suppress_callback) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        context->pressed = true;
        context->dragging = false;
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
            lv_obj_invalidate(context->slider);
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
        const int32_t value = pointer_value(context);
        if (was_dragging) {
            apply_pointer(context, true);
        }
        else {
            set_native_value(context, value);
        }
        context->pressed = false;
        context->dragging = false;
        context->committed_value = (uint8_t)value;
        context->power_on = context->committed_value > 0;
        update_tip(context);
        if (was_dragging) {
            lv_obj_invalidate(context->slider);
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
        set_visual_value(context, context->committed_value);
        update_tip(context);
        if (context->callback) {
            context->callback(context->committed_value,
                              LIGHT_CONTROL_CANCEL,
                              context->user_data);
        }
    }
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
    context->display_color = contrast_color(lv_color_hex(0xFFC107));

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
            lv_obj_set_style_text_color(context->tip_label,
                                        lv_color_white(),
                                        0);
            lv_obj_center(context->tip_label);
        }
        lv_obj_add_flag(context->tip, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_ALL, context);
    lv_obj_refresh_ext_draw_size(slider);
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
        lv_obj_invalidate(obj);
    }
}

void light_brightness_slider_set_active_color(lv_obj_t *obj, lv_color_t color)
{
    light_brightness_slider_context_t *context = get_context(obj);
    if (context) {
        context->display_color = contrast_color(color);
        lv_obj_invalidate(obj);
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
        lv_obj_invalidate(obj);
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
    lv_obj_invalidate(obj);
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
