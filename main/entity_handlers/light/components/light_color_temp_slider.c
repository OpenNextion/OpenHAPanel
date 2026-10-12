#include "light_color_temp_slider.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define COLOR_TEMP_SLIDER_WIDTH         88
#define COLOR_TEMP_SLIDER_HEIGHT        230
#define COLOR_TEMP_SLIDER_RADIUS        20
#define COLOR_TEMP_CURSOR_HEIGHT        22
#define COLOR_TEMP_CURSOR_RADIUS        4
#define COLOR_TEMP_CURSOR_LINE_WIDTH    44
#define COLOR_TEMP_CURSOR_LINE_Y        9
#define COLOR_TEMP_DRAG_THRESHOLD       3
#define COLOR_TEMP_HOLD_DELAY_MS        220
#define COLOR_TEMP_TRANSITION_MS        180
#define COLOR_TEMP_GRADIENT_SEGMENTS    10
#define COLOR_TEMP_GRADIENT_STOP_COUNT  11
#define COLOR_TEMP_EXT_DRAW_SIZE         80

typedef struct {
    lv_obj_t *slider;
    lv_obj_t *tip;
    lv_obj_t *tip_label;
    uint16_t min_kelvin;
    uint16_t max_kelvin;
    uint16_t committed_value;
    int32_t visual_value;
    uint32_t gradient_stops[COLOR_TEMP_GRADIENT_STOP_COUNT];
    lv_point_t press_origin;
    uint32_t press_tick;
    bool pressed;
    bool dragging;
    bool marker_visible;
    bool suppress_callback;
    bool disabled;
    light_color_temp_change_cb_t callback;
    void *user_data;
} light_color_temp_slider_context_t;

static light_color_temp_slider_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static int32_t clamp_value(int32_t value, int32_t minimum, int32_t maximum)
{
    return LV_CLAMP(minimum, value, maximum);
}

/* Same approximation used by the Floor lamp demo. */
static uint32_t kelvin_color(float kelvin)
{
    const float temperature = kelvin / 100.0f;
    const float red = temperature <= 66.0f
                          ? 255.0f
                          : 329.698727446f *
                                powf(temperature - 60.0f, -0.1332047592f);
    const float green = temperature <= 66.0f
                            ? 99.4708025861f * logf(temperature) -
                                  161.1195681661f
                            : 288.1221695283f *
                                  powf(temperature - 60.0f,
                                       -0.0755148492f);
    const float blue = temperature >= 66.0f
                           ? 255.0f
                           : temperature <= 19.0f
                                 ? 0.0f
                                 : 138.5177312231f *
                                           logf(temperature - 10.0f) -
                                       305.0447927307f;
    const uint32_t r = (uint32_t)clamp_value((int32_t)lroundf(red), 0, 255);
    const uint32_t g = (uint32_t)clamp_value((int32_t)lroundf(green), 0, 255);
    const uint32_t b = (uint32_t)clamp_value((int32_t)lroundf(blue), 0, 255);
    return (r << 16) | (g << 8) | b;
}

static void update_gradient_stops(light_color_temp_slider_context_t *context)
{
    const uint32_t span = context->max_kelvin - context->min_kelvin;
    for (size_t i = 0; i < COLOR_TEMP_GRADIENT_STOP_COUNT; i++) {
        const float kelvin =
            context->min_kelvin +
            span * (float)i / COLOR_TEMP_GRADIENT_SEGMENTS;
        context->gradient_stops[i] = kelvin_color(kelvin);
    }
}

static bool area_is_visible(lv_layer_t *layer, const lv_area_t *area)
{
    const lv_area_t *clip = &layer->_clip_area;
    return area->x1 <= clip->x2 && area->x2 >= clip->x1 &&
           area->y1 <= clip->y2 && area->y2 >= clip->y1;
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
    lv_area_t saved_clip = layer->_clip_area;
    lv_area_t clip = {
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
        }
        y = end + 1;
    }
    layer->_clip_area = saved_clip;
}

static void draw_gradient(lv_layer_t *layer,
                          const lv_area_t *area,
                          void *user_data)
{
    light_color_temp_slider_context_t *context = user_data;
    const int32_t height = lv_area_get_height(area);
    const lv_opa_t opacity = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    unsigned pending = 0;
    for (size_t i = 0; i < COLOR_TEMP_GRADIENT_SEGMENTS; i++) {
        lv_area_t band = {
            .x1 = area->x1,
            .y1 = area->y1 + (int32_t)i * height /
                                  COLOR_TEMP_GRADIENT_SEGMENTS,
            .x2 = area->x2,
            .y2 = area->y1 + (int32_t)(i + 1) * height /
                                  COLOR_TEMP_GRADIENT_SEGMENTS -
                  1,
        };
        if (!area_is_visible(layer, &band)) {
            continue;
        }
        lv_draw_rect_dsc_t gradient;
        lv_draw_rect_dsc_init(&gradient);
        gradient.bg_opa = opacity;
        gradient.bg_color = lv_color_hex(context->gradient_stops[i]);
        gradient.bg_grad.dir = LV_GRAD_DIR_VER;
        gradient.bg_grad.stops_count = 2;
        gradient.bg_grad.stops[0] = (lv_grad_stop_t) {
            .color = lv_color_hex(context->gradient_stops[i]),
            .opa = opacity,
            .frac = 0,
        };
        gradient.bg_grad.stops[1] = (lv_grad_stop_t) {
            .color = lv_color_hex(context->gradient_stops[i + 1]),
            .opa = opacity,
            .frac = 255,
        };
        lv_draw_rect(layer, &gradient, &band);
        if (++pending == 2) {
            finish_draw_batch(layer);
            pending = 0;
        }
    }
    if (pending > 0) {
        finish_draw_batch(layer);
    }
}

static int32_t value_to_cursor_y(
    const light_color_temp_slider_context_t *context,
    const lv_area_t *area,
    int32_t value)
{
    const int32_t range = context->max_kelvin - context->min_kelvin;
    const int32_t travel =
        lv_area_get_height(area) - COLOR_TEMP_CURSOR_HEIGHT;
    if (range <= 0 || travel <= 0) {
        return area->y1;
    }
    return area->y1 +
           (value - context->min_kelvin) * travel / range;
}

static void draw_cursor(light_color_temp_slider_context_t *context,
                        lv_layer_t *layer,
                        const lv_area_t *area)
{
    if (!context->marker_visible && !context->dragging) {
        return;
    }
    const int32_t y = value_to_cursor_y(context,
                                        area,
                                        context->visual_value);
    lv_area_t cursor = {
        .x1 = area->x1,
        .y1 = y,
        .x2 = area->x2,
        .y2 = y + COLOR_TEMP_CURSOR_HEIGHT - 1,
    };
    lv_draw_rect_dsc_t cursor_style;
    lv_draw_rect_dsc_init(&cursor_style);
    cursor_style.bg_color = lv_color_white();
    cursor_style.bg_opa = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    cursor_style.radius = COLOR_TEMP_CURSOR_RADIUS;
    lv_draw_rect(layer, &cursor_style, &cursor);

    const int32_t center_x =
        area->x1 + lv_area_get_width(area) / 2;
    lv_area_t line = {
        .x1 = center_x - COLOR_TEMP_CURSOR_LINE_WIDTH / 2,
        .y1 = y + COLOR_TEMP_CURSOR_LINE_Y,
        .x2 = center_x + COLOR_TEMP_CURSOR_LINE_WIDTH / 2 - 1,
        .y2 = y + COLOR_TEMP_CURSOR_LINE_Y + 3,
    };
    lv_draw_rect_dsc_t line_style;
    lv_draw_rect_dsc_init(&line_style);
    line_style.bg_color = lv_color_hex(0x666666);
    line_style.bg_opa = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    line_style.radius = 2;
    lv_draw_rect(layer, &line_style, &line);
}

static void draw_slider(lv_layer_t *layer,
                        const lv_area_t *area,
                        void *user_data)
{
    light_color_temp_slider_context_t *context = user_data;
    draw_gradient(layer, area, context);
    draw_cursor(context, layer, area);
}

static void draw_cb(lv_event_t *event)
{
    light_color_temp_slider_context_t *context =
        lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    lv_area_t area;
    lv_obj_get_coords(context->slider, &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    draw_with_rounded_clip(layer,
                           &area,
                           COLOR_TEMP_SLIDER_RADIUS,
                           draw_slider,
                           context);
}

static void update_tip(light_color_temp_slider_context_t *context)
{
    if (!context->tip || !context->tip_label) {
        return;
    }
    if (!context->dragging || context->disabled) {
        lv_obj_add_flag(context->tip, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char text[24];
    snprintf(text, sizeof(text), "%ld K", (long)context->visual_value);
    lv_label_set_text(context->tip_label, text);
    lv_obj_remove_flag(context->tip, LV_OBJ_FLAG_HIDDEN);

    const int32_t range = context->max_kelvin - context->min_kelvin;
    const int32_t travel = COLOR_TEMP_SLIDER_HEIGHT -
                           COLOR_TEMP_CURSOR_HEIGHT;
    int32_t y = range > 0
                    ? (context->visual_value - context->min_kelvin) *
                              travel / range +
                          COLOR_TEMP_CURSOR_HEIGHT / 2 - 18
                    : 0;
    y = clamp_value(y, 0, COLOR_TEMP_SLIDER_HEIGHT - 36);
    lv_obj_set_pos(context->tip, -76, y);
    lv_obj_move_foreground(context->tip);
}

static void set_native_value(light_color_temp_slider_context_t *context,
                             int32_t value)
{
    context->suppress_callback = true;
    lv_slider_set_value(context->slider, value, LV_ANIM_OFF);
    context->suppress_callback = false;
}

static void set_visual_value(light_color_temp_slider_context_t *context,
                             int32_t value)
{
    context->visual_value = clamp_value(value,
                                        context->min_kelvin,
                                        context->max_kelvin);
    set_native_value(context, context->visual_value);
    lv_obj_invalidate(context->slider);
    update_tip(context);
}

static int32_t pointer_value(light_color_temp_slider_context_t *context)
{
    lv_indev_t *input = lv_indev_active();
    if (!input) {
        return context->visual_value;
    }
    lv_point_t point;
    lv_indev_get_point(input, &point);
    lv_area_t area;
    lv_obj_get_coords(context->slider, &area);
    const int32_t range = context->max_kelvin - context->min_kelvin;
    const int32_t travel =
        lv_area_get_height(&area) - COLOR_TEMP_CURSOR_HEIGHT;
    if (range <= 0 || travel <= 0) {
        return context->min_kelvin;
    }
    const float position =
        (float)(point.y - area.y1 - COLOR_TEMP_CURSOR_HEIGHT / 2);
    const int32_t value =
        context->min_kelvin + (int32_t)lroundf(position * range / travel);
    return clamp_value(value,
                       context->min_kelvin,
                       context->max_kelvin);
}

static void apply_pointer(light_color_temp_slider_context_t *context,
                          bool preview)
{
    const int32_t value = pointer_value(context);
    const bool changed = value != context->visual_value;
    set_visual_value(context, value);
    if (preview && changed && context->callback) {
        context->callback((uint16_t)value,
                          LIGHT_CONTROL_PREVIEW,
                          context->user_data);
    }
}

static void visual_anim_cb(void *var, int32_t value)
{
    light_color_temp_slider_context_t *context = var;
    if (!context || !context->slider) {
        return;
    }
    context->visual_value = value;
    lv_obj_invalidate(context->slider);
}

static void event_cb(lv_event_t *event)
{
    light_color_temp_slider_context_t *context =
        lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(event, COLOR_TEMP_EXT_DRAW_SIZE);
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
                 COLOR_TEMP_DRAG_THRESHOLD ||
             lv_tick_elaps(context->press_tick) >=
                 COLOR_TEMP_HOLD_DELAY_MS)) {
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
            const int32_t value = lv_slider_get_value(context->slider);
            context->visual_value = value;
            lv_obj_invalidate(context->slider);
            if (context->callback) {
                context->callback((uint16_t)value,
                                  LIGHT_CONTROL_PREVIEW,
                                  context->user_data);
            }
        }
        return;
    }
    if (code == LV_EVENT_RELEASED && context->pressed) {
        apply_pointer(context, context->dragging);
        context->pressed = false;
        context->dragging = false;
        context->committed_value = (uint16_t)context->visual_value;
        context->marker_visible = true;
        update_tip(context);
        lv_obj_invalidate(context->slider);
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

lv_obj_t *light_color_temp_slider_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_color_temp_slider_context_t *context =
        lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }
    context->min_kelvin = 2000;
    context->max_kelvin = 6500;
    context->committed_value = 4000;
    context->visual_value = context->committed_value;
    update_gradient_stops(context);

    lv_obj_t *slider = lv_slider_create(parent);
    if (!slider) {
        lv_free(context);
        return NULL;
    }
    context->slider = slider;
    lv_obj_set_user_data(slider, context);
    lv_obj_set_size(slider,
                    COLOR_TEMP_SLIDER_WIDTH,
                    COLOR_TEMP_SLIDER_HEIGHT);
    lv_slider_set_orientation(slider, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider,
                        context->min_kelvin,
                        context->max_kelvin);
    lv_slider_set_value(slider, context->committed_value, LV_ANIM_OFF);
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

void light_color_temp_slider_set_range(lv_obj_t *obj,
                                       uint16_t min_kelvin,
                                       uint16_t max_kelvin)
{
    light_color_temp_slider_context_t *context = get_context(obj);
    if (!context || min_kelvin >= max_kelvin) {
        return;
    }
    context->min_kelvin = min_kelvin;
    context->max_kelvin = max_kelvin;
    context->committed_value =
        (uint16_t)clamp_value(context->committed_value,
                              min_kelvin,
                              max_kelvin);
    context->visual_value = context->committed_value;
    update_gradient_stops(context);
    lv_slider_set_range(obj, min_kelvin, max_kelvin);
    set_native_value(context, context->committed_value);
    lv_obj_invalidate(obj);
}

void light_color_temp_slider_set_value(lv_obj_t *obj,
                                       uint16_t kelvin,
                                       lv_anim_enable_t animate)
{
    light_color_temp_slider_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    kelvin = (uint16_t)clamp_value(kelvin,
                                   context->min_kelvin,
                                   context->max_kelvin);
    context->committed_value = kelvin;
    set_native_value(context, kelvin);
    lv_anim_delete(context, visual_anim_cb);
    if (animate == LV_ANIM_ON && !context->dragging &&
        context->visual_value != kelvin) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, context);
        lv_anim_set_exec_cb(&animation, visual_anim_cb);
        lv_anim_set_values(&animation, context->visual_value, kelvin);
        lv_anim_set_duration(&animation, COLOR_TEMP_TRANSITION_MS);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
        lv_anim_start(&animation);
    }
    else {
        context->visual_value = kelvin;
        lv_obj_invalidate(obj);
    }
}

void light_color_temp_slider_set_marker_visible(lv_obj_t *obj, bool visible)
{
    light_color_temp_slider_context_t *context = get_context(obj);
    if (context) {
        context->marker_visible = visible;
        lv_obj_invalidate(obj);
    }
}

void light_color_temp_slider_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_color_temp_slider_context_t *context = get_context(obj);
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

void light_color_temp_slider_set_change_cb(
    lv_obj_t *obj,
    light_color_temp_change_cb_t callback,
    void *user_data)
{
    light_color_temp_slider_context_t *context = get_context(obj);
    if (context) {
        context->callback = callback;
        context->user_data = user_data;
    }
}
