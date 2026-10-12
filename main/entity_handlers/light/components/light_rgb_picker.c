#include "light_rgb_picker.h"

#include <math.h>

#include "assets/ha_ui_assets.h"

#define RGB_PICKER_SIZE                 230
#define RGB_PICKER_CENTER               (RGB_PICKER_SIZE / 2)
#define RGB_PICKER_MARKER_LIMIT         111
#define RGB_PICKER_MARKER_BASE_RADIUS   10
#define RGB_PICKER_MARKER_DRAG_RADIUS   24
#define RGB_PICKER_MARKER_LIFT          15
#define RGB_PICKER_EXT_DRAW_SIZE        44
#define RGB_PICKER_DRAG_THRESHOLD       3
#define RGB_PICKER_HOLD_DELAY_MS        220
#define RGB_PICKER_RELEASE_MS           100
#define RGB_PICKER_SCALE_NORMAL         100
#define RGB_PICKER_SCALE_DRAG           250
#define RGB_PICKER_PI                   3.14159265f

typedef struct {
    lv_obj_t *root;
    light_rgb_t committed_value;
    light_rgb_t preview_value;
    int16_t marker_x;
    int16_t marker_y;
    int16_t committed_marker_x;
    int16_t committed_marker_y;
    int32_t marker_scale;
    lv_point_t press_origin;
    uint32_t press_tick;
    bool marker_visible;
    bool marker_visible_before_press;
    bool pressed;
    bool dragging;
    bool disabled;
    light_rgb_change_cb_t callback;
    void *user_data;
} light_rgb_picker_context_t;

static light_rgb_picker_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static bool rgb_equal(light_rgb_t left, light_rgb_t right)
{
    return left.red == right.red &&
           left.green == right.green &&
           left.blue == right.blue;
}

static lv_color_t rgb_color(light_rgb_t value)
{
    return lv_color_make(value.red, value.green, value.blue);
}

static light_rgb_t hsv_to_rgb(float hue, float saturation)
{
    const float h = hue / 60.0f;
    const float chroma = saturation;
    const float x = chroma *
                    (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
    const float m = 1.0f - chroma;
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    if (h < 1.0f) {
        red = chroma;
        green = x;
    }
    else if (h < 2.0f) {
        red = x;
        green = chroma;
    }
    else if (h < 3.0f) {
        green = chroma;
        blue = x;
    }
    else if (h < 4.0f) {
        green = x;
        blue = chroma;
    }
    else if (h < 5.0f) {
        red = x;
        blue = chroma;
    }
    else {
        red = chroma;
        blue = x;
    }

    return (light_rgb_t) {
        .red = (uint8_t)lroundf((red + m) * 255.0f),
        .green = (uint8_t)lroundf((green + m) * 255.0f),
        .blue = (uint8_t)lroundf((blue + m) * 255.0f),
    };
}

static void rgb_to_marker(light_rgb_t rgb, int16_t *x, int16_t *y)
{
    const float red = rgb.red;
    const float green = rgb.green;
    const float blue = rgb.blue;
    const float high = LV_MAX(red, LV_MAX(green, blue));
    const float low = LV_MIN(red, LV_MIN(green, blue));
    const float chroma = high - low;
    float hue_sector = 0.0f;

    if (chroma > 0.0f) {
        if (high == red) {
            hue_sector = fmodf((green - blue) / chroma + 6.0f,
                               6.0f);
        }
        else if (high == green) {
            hue_sector = (blue - red) / chroma + 2.0f;
        }
        else {
            hue_sector = (red - green) / chroma + 4.0f;
        }
    }

    const float saturation = high > 0.0f ? chroma / high : 0.0f;
    const float radius = LV_MIN((float)RGB_PICKER_MARKER_LIMIT,
                                RGB_PICKER_CENTER * saturation);
    /* rgb_color_230 starts at red on the right and advances clockwise. */
    const float angle = hue_sector * RGB_PICKER_PI / 3.0f;
    *x = (int16_t)lroundf(RGB_PICKER_CENTER + radius * cosf(angle));
    *y = (int16_t)lroundf(RGB_PICKER_CENTER + radius * sinf(angle));
}

static lv_area_t marker_area(const light_rgb_picker_context_t *context)
{
    lv_area_t root_area;
    lv_obj_get_coords(context->root, &root_area);
    const int32_t scale = LV_MAX(context->marker_scale,
                                 RGB_PICKER_SCALE_NORMAL);
    const int32_t radius = RGB_PICKER_MARKER_BASE_RADIUS +
                           (scale - RGB_PICKER_SCALE_NORMAL) *
                               (RGB_PICKER_MARKER_DRAG_RADIUS -
                                RGB_PICKER_MARKER_BASE_RADIUS) /
                               (RGB_PICKER_SCALE_DRAG -
                                RGB_PICKER_SCALE_NORMAL);
    const int32_t lift = (scale - RGB_PICKER_SCALE_NORMAL) *
                         RGB_PICKER_MARKER_LIFT /
                         (RGB_PICKER_SCALE_DRAG -
                          RGB_PICKER_SCALE_NORMAL);
    const int32_t center_x = root_area.x1 + context->marker_x;
    const int32_t center_y = root_area.y1 + context->marker_y - lift;
    return (lv_area_t) {
        .x1 = center_x - radius,
        .y1 = center_y - radius,
        .x2 = center_x + radius - 1,
        .y2 = center_y + radius - 1,
    };
}

static void invalidate_marker_area(light_rgb_picker_context_t *context,
                                   lv_area_t area)
{
    if (!context || !context->root) {
        return;
    }
    lv_area_increase(&area, 3, 3);
    lv_obj_invalidate_area(context->root, &area);
}

static void set_marker_scale(void *var, int32_t scale)
{
    light_rgb_picker_context_t *context = var;
    if (!context || !context->root) {
        return;
    }
    lv_area_t old_area = marker_area(context);
    context->marker_scale = scale;
    invalidate_marker_area(context, old_area);
    invalidate_marker_area(context, marker_area(context));
}

static void animate_marker_release(light_rgb_picker_context_t *context)
{
    context->pressed = false;
    context->dragging = false;
    if (context->marker_scale == RGB_PICKER_SCALE_NORMAL) {
        return;
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, context);
    lv_anim_set_exec_cb(&animation, set_marker_scale);
    lv_anim_set_values(&animation,
                       context->marker_scale,
                       RGB_PICKER_SCALE_NORMAL);
    lv_anim_set_duration(&animation, RGB_PICKER_RELEASE_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}

static bool get_pointer(lv_point_t *point)
{
    lv_indev_t *input = lv_indev_active();
    if (!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) {
        return false;
    }
    lv_indev_get_point(input, point);
    return true;
}

static bool apply_pointer(light_rgb_picker_context_t *context,
                          const lv_point_t *point,
                          bool notify_preview)
{
    lv_area_t root_area;
    lv_obj_get_coords(context->root, &root_area);
    const float dx = point->x - root_area.x1 - RGB_PICKER_CENTER;
    const float dy = point->y - root_area.y1 - RGB_PICKER_CENTER;
    const float distance = hypotf(dx, dy);
    const float saturation = LV_MIN(distance / RGB_PICKER_CENTER, 1.0f);
    /* Match rgb_color_230: 0 degrees/red is at the wheel's right edge. */
    const float hue = fmodf(atan2f(dy, dx) * 180.0f /
                                RGB_PICKER_PI +
                            360.0f,
                            360.0f);
    const light_rgb_t value = hsv_to_rgb(hue, saturation);
    const float marker_ratio =
        distance > RGB_PICKER_MARKER_LIMIT && distance > 0.0f
            ? RGB_PICKER_MARKER_LIMIT / distance
            : 1.0f;
    const int16_t marker_x = (int16_t)lroundf(
        RGB_PICKER_CENTER + dx * marker_ratio);
    const int16_t marker_y = (int16_t)lroundf(
        RGB_PICKER_CENTER + dy * marker_ratio);
    const bool changed = !rgb_equal(value, context->preview_value) ||
                         marker_x != context->marker_x ||
                         marker_y != context->marker_y;
    if (!changed) {
        return false;
    }

    lv_area_t old_area = marker_area(context);
    context->preview_value = value;
    context->marker_x = marker_x;
    context->marker_y = marker_y;
    invalidate_marker_area(context, old_area);
    invalidate_marker_area(context, marker_area(context));

    if (notify_preview && context->callback) {
        context->callback(value,
                          LIGHT_CONTROL_PREVIEW,
                          context->user_data);
    }
    return true;
}

static void restore_committed_value(light_rgb_picker_context_t *context)
{
    lv_area_t old_area = marker_area(context);
    context->preview_value = context->committed_value;
    context->marker_x = context->committed_marker_x;
    context->marker_y = context->committed_marker_y;
    context->marker_visible = context->marker_visible_before_press;
    invalidate_marker_area(context, old_area);
    invalidate_marker_area(context, marker_area(context));
}

static void draw_marker(lv_event_t *event,
                        light_rgb_picker_context_t *context)
{
    if (!context->marker_visible && !context->pressed) {
        return;
    }

    const int32_t scale = LV_MAX(context->marker_scale,
                                 RGB_PICKER_SCALE_NORMAL);
    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = rgb_color(context->preview_value);
    descriptor.bg_opa = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    descriptor.radius = LV_RADIUS_CIRCLE;
    descriptor.border_color = lv_color_white();
    descriptor.border_width = (16 * scale + 500) / 1000;
    descriptor.border_opa = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    const lv_area_t area = marker_area(context);
    lv_draw_rect(lv_event_get_layer(event), &descriptor, &area);
}

static void handle_press(light_rgb_picker_context_t *context,
                         lv_event_code_t code)
{
    lv_point_t point;
    if (!get_pointer(&point)) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_anim_delete(context, set_marker_scale);
        set_marker_scale(context, RGB_PICKER_SCALE_NORMAL);
        context->pressed = true;
        context->dragging = false;
        context->marker_visible_before_press = context->marker_visible;
        context->press_origin = point;
        context->press_tick = lv_tick_get();
        invalidate_marker_area(context, marker_area(context));
    }
    else if (!context->pressed) {
        return;
    }

    if (!context->dragging &&
        (LV_ABS(point.x - context->press_origin.x) +
                 LV_ABS(point.y - context->press_origin.y) >
             RGB_PICKER_DRAG_THRESHOLD ||
         lv_tick_elaps(context->press_tick) >=
             RGB_PICKER_HOLD_DELAY_MS)) {
        context->dragging = true;
        set_marker_scale(context, RGB_PICKER_SCALE_DRAG);
    }
    apply_pointer(context, &point, true);
}

static void handle_release(light_rgb_picker_context_t *context)
{
    if (!context->pressed) {
        return;
    }

    lv_point_t point;
    if (get_pointer(&point)) {
        apply_pointer(context, &point, false);
    }
    context->committed_value = context->preview_value;
    context->committed_marker_x = context->marker_x;
    context->committed_marker_y = context->marker_y;
    context->marker_visible = true;
    if (context->callback) {
        context->callback(context->committed_value,
                          LIGHT_CONTROL_COMMIT,
                          context->user_data);
    }
    animate_marker_release(context);
}

static void handle_press_lost(light_rgb_picker_context_t *context)
{
    if (!context->pressed) {
        return;
    }
    restore_committed_value(context);
    if (context->callback) {
        context->callback(context->committed_value,
                          LIGHT_CONTROL_CANCEL,
                          context->user_data);
    }
    animate_marker_release(context);
}

static void event_cb(lv_event_t *event)
{
    light_rgb_picker_context_t *context = lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        int32_t *extra = lv_event_get_param(event);
        *extra = LV_MAX(*extra, RGB_PICKER_EXT_DRAW_SIZE);
        return;
    }
    if (code == LV_EVENT_DELETE) {
        lv_anim_delete(context, set_marker_scale);
        context->root = NULL;
        lv_free(context);
        return;
    }
    if (code == LV_EVENT_DRAW_POST) {
        draw_marker(event, context);
        return;
    }
    if (context->disabled) {
        return;
    }
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        handle_press(context, code);
    }
    else if (code == LV_EVENT_RELEASED) {
        handle_release(context);
    }
    else if (code == LV_EVENT_PRESS_LOST) {
        handle_press_lost(context);
    }
}

lv_obj_t *light_rgb_picker_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_rgb_picker_context_t *context = lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }

    context->committed_value = (light_rgb_t) {255, 255, 255};
    context->preview_value = context->committed_value;
    context->marker_x = RGB_PICKER_CENTER;
    context->marker_y = RGB_PICKER_CENTER;
    context->committed_marker_x = context->marker_x;
    context->committed_marker_y = context->marker_y;
    context->marker_scale = RGB_PICKER_SCALE_NORMAL;

    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    context->root = root;
    lv_obj_set_user_data(root, context);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, RGB_PICKER_SIZE, RGB_PICKER_SIZE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(root,
                       LV_OBJ_FLAG_SCROLLABLE |
                           LV_OBJ_FLAG_SCROLL_CHAIN |
                           LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_style_bg_image_src(root, &rgb_color_230, 0);
    lv_obj_set_style_bg_image_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(root, event_cb, LV_EVENT_ALL, context);
    lv_obj_refresh_ext_draw_size(root);
    return root;
}

void light_rgb_picker_set_value(lv_obj_t *obj, light_rgb_t rgb)
{
    light_rgb_picker_context_t *context = get_context(obj);
    if (!context) {
        return;
    }

    lv_area_t old_area = marker_area(context);
    context->committed_value = rgb;
    context->preview_value = rgb;
    rgb_to_marker(rgb, &context->marker_x, &context->marker_y);
    context->committed_marker_x = context->marker_x;
    context->committed_marker_y = context->marker_y;
    invalidate_marker_area(context, old_area);
    invalidate_marker_area(context, marker_area(context));
}

void light_rgb_picker_set_marker_visible(lv_obj_t *obj, bool visible)
{
    light_rgb_picker_context_t *context = get_context(obj);
    if (!context || context->marker_visible == visible) {
        return;
    }
    invalidate_marker_area(context, marker_area(context));
    context->marker_visible = visible;
}

void light_rgb_picker_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_rgb_picker_context_t *context = get_context(obj);
    if (!context || context->disabled == disabled) {
        return;
    }

    if (disabled) {
        lv_anim_delete(context, set_marker_scale);
        if (context->pressed) {
            restore_committed_value(context);
        }
        context->pressed = false;
        context->dragging = false;
        set_marker_scale(context, RGB_PICKER_SCALE_NORMAL);
    }
    context->disabled = disabled;
    lv_obj_set_style_bg_image_opa(obj,
                                  disabled ? LV_OPA_40 : LV_OPA_COVER,
                                  0);
    if (disabled) {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
    else {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    }
    lv_obj_invalidate(obj);
}

void light_rgb_picker_set_change_cb(lv_obj_t *obj,
                                    light_rgb_change_cb_t callback,
                                    void *user_data)
{
    light_rgb_picker_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->callback = callback;
    context->user_data = user_data;
}
