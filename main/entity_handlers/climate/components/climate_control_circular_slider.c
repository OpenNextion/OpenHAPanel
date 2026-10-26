/**
 * @file climate_control_circular_slider.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_control_circular_slider.h"
#include "climate_control_circular_slider_gen.h"
#include "climate_control_circular_slider_private_gen.h"
#include <lvgl.h>
#include "src/core/lv_obj_event.h"
#include "src/core/lv_obj_pos.h"
#include "src/core/lv_obj_style.h"
#include "src/core/lv_obj_style_gen.h"
#include "src/misc/lv_color.h"
#include "src/misc/lv_event.h"
#include "src/indev/lv_indev.h"
#include "src/widgets/arc/lv_arc.h"
#include "src/widgets/label/lv_label.h"

/*********************
 *      DEFINES
 *********************/

#define CLIMATE_CIRCULAR_SLIDER_DEFAULT_SIZE 320
#define CLIMATE_CIRCULAR_SLIDER_MIN_SIZE     80
#define CLIMATE_CIRCULAR_SLIDER_LINE_WIDTH   24
#define CLIMATE_CIRCULAR_SLIDER_KNOB_SIZE    18
#define CLIMATE_CIRCULAR_SLIDER_TOUCH_EXTRA  24
#define CLIMATE_CIRCULAR_SLIDER_EXT_CLICK    32
#define CLIMATE_CIRCULAR_SLIDER_SIDE_OVERLAP 40

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t clamp_i32(int32_t value, int32_t min, int32_t max)
{
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool is_low_active(climate_control_circular_slider_t * root)
{
    int32_t touch_value = lv_arc_get_value(root->touch_arc);
    int32_t current = root->current;
    return touch_value <= current;
}

int32_t scale_dim(int32_t size, int32_t base)
{
    int32_t scaled = (size * base + CLIMATE_CIRCULAR_SLIDER_DEFAULT_SIZE / 2) / CLIMATE_CIRCULAR_SLIDER_DEFAULT_SIZE;
    return scaled < 1 ? 1 : scaled;
}

int32_t value_to_angle(const climate_control_circular_slider_t * state, int32_t value)
{
    int32_t range = state->max - state->min;
    if(range <= 0) return 0;

    value = clamp_i32(value, state->min, state->max);
    return ((value - state->min) * 270) / range;
}

int32_t point_to_value(const climate_control_circular_slider_t * state, lv_obj_t * arc, const lv_point_t * point)
{
    lv_area_t coords;
    lv_obj_get_coords(arc, &coords);
    lv_coord_t cx = coords.x1 + lv_area_get_width(&coords) / 2;
    lv_coord_t cy = coords.y1 + lv_area_get_height(&coords) / 2;
    lv_value_precise_t angle = lv_atan2(point->y - cy, point->x - cx);

    angle -= 135;
    while(angle < 0) angle += 360;
    while(angle >= 360) angle -= 360;
    if(angle > 270) {
        angle = angle > 315 ? 0 : 270;
    }

    int32_t range = state->max - state->min;
    if(range <= 0) return state->min;
    return state->min + ((int32_t)angle * range + 135) / 270;
}

int32_t snap_value_to_step(const climate_control_circular_slider_t * state, int32_t value, int32_t min, int32_t max)
{
    int32_t step = state->slider_step < 1 ? 1 : state->slider_step;
    value = clamp_i32(value, min, max);
    value = state->min + (((value - state->min) + step / 2) / step) * step;
    return clamp_i32(value, min, max);
}

void apply_dual_visibility(climate_control_circular_slider_t * root)
{
    if(root->dual) {
        lv_obj_remove_flag(root->high_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(root->high_active_arc, LV_OBJ_FLAG_HIDDEN);
        if(root->show_knob) {
            lv_obj_remove_flag(root->high_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(root->high_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(root->high_active_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(root->high_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
        root->updating_high = false;
    }
}

void apply_value_arc_range(climate_control_circular_slider_t * root)
{
    lv_obj_t * arcs[] = {
        root->cur_dot_knob_arc,
        root->low_dot_knob_arc,
        root->high_dot_knob_arc,
        root->touch_arc,
    };

    for(uint32_t i = 0; i < sizeof(arcs) / sizeof(arcs[0]); i++) {
        if(arcs[i] == NULL) continue;
        lv_arc_set_min_value(arcs[i], root->min);
        lv_arc_set_max_value(arcs[i], root->max);
    }
}

void apply_touch_arc_value(climate_control_circular_slider_t * root, int32_t value)
{
    lv_arc_set_value(root->touch_arc, clamp_i32(value, root->min, root->max));
}

void refresh_value_arcs(climate_control_circular_slider_t * root)
{
    int32_t c_angle = value_to_angle(root, root->current);
    int32_t target_low_val = root->dual ? root->low : root->value;
    int32_t low_angle = value_to_angle(root, target_low_val);
    int32_t high_angle = root->dual ? value_to_angle(root, root->high) : 270;

    if (!root->dual) {
        if (root->mode == CLIMATE_CIRCULAR_SLIDER_MODE_NONE) {
            lv_arc_set_angles(root->low_arc, 0, 0);
            lv_arc_set_angles(root->low_active_arc, 0, 0);
        } else if (root->mode == CLIMATE_CIRCULAR_SLIDER_MODE_FULL) {
            lv_arc_set_angles(root->low_arc, 0, 270);
            lv_arc_set_angles(root->low_active_arc, 0, 0);
        } else if (root->mode == CLIMATE_CIRCULAR_SLIDER_MODE_END) {
            lv_arc_set_angles(root->low_arc, low_angle, 270);
            if (target_low_val >= root->current) {
                lv_arc_set_angles(root->low_active_arc, 0, 0);
            } else {
                lv_arc_set_angles(root->low_active_arc, clamp_i32(low_angle, 0, c_angle), c_angle);
            }
        } else {
            lv_arc_set_angles(root->low_arc, 0, low_angle);
            if (target_low_val <= root->current) {
                lv_arc_set_angles(root->low_active_arc, 0, 0);
            } else {
                lv_arc_set_angles(root->low_active_arc, c_angle, clamp_i32(low_angle, c_angle, 270));
            }
        }
    } else {
        lv_arc_set_angles(root->low_arc, 0, clamp_i32(low_angle, 0, c_angle));
        if(target_low_val <= root->current) {
            lv_arc_set_angles(root->low_active_arc, 0, 0);
        } else {
            lv_arc_set_angles(root->low_active_arc, c_angle, clamp_i32(low_angle, c_angle, high_angle));
        }

        lv_arc_set_angles(root->high_arc, clamp_i32(high_angle, c_angle, 270), 270);
        if(root->high > root->current) {
            lv_arc_set_angles(root->high_active_arc, 0, 0);
        } else {
            lv_arc_set_angles(root->high_active_arc, clamp_i32(high_angle, low_angle, c_angle), c_angle);
        }
    }

    bool is_on_colored_arc = false;
    if (root->dual) {
        is_on_colored_arc = (root->current <= root->low) || (root->current >= root->high);
    } else {
        if (root->mode == CLIMATE_CIRCULAR_SLIDER_MODE_NONE) {
            is_on_colored_arc = false;
        } else if (root->mode == CLIMATE_CIRCULAR_SLIDER_MODE_FULL) {
            is_on_colored_arc = true;
        } else if (root->mode == CLIMATE_CIRCULAR_SLIDER_MODE_END) {
            is_on_colored_arc = (root->current >= root->value);
        } else {
            is_on_colored_arc = (root->current <= root->value);
        }
    }

    if (is_on_colored_arc) {
        lv_obj_set_style_bg_color(root->cur_dot_knob_arc, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(root->cur_dot_knob_arc, LV_OPA_50, LV_PART_KNOB);
    } else {
        lv_obj_set_style_bg_color(root->cur_dot_knob_arc, lv_color_hex(0x959595), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(root->cur_dot_knob_arc, LV_OPA_COVER, LV_PART_KNOB);
    }
}

static void pointer_begin(climate_control_circular_slider_t * root, int32_t value)
{
    value = clamp_i32(value, root->min, root->max);
    int32_t low_target = root->dual ? root->low : root->value;
    int32_t low_distance = LV_ABS(value - low_target);
    int32_t high_distance = LV_ABS(value - root->high);
    bool choose_low = !root->dual ||
                      (low_distance == high_distance ? value < root->low
                                                     : low_distance < high_distance);
    root->updating_low = choose_low;
    root->updating_high = !choose_low;
}

static void pointer_update(climate_control_circular_slider_t * root, int32_t value)
{
    climate_control_circular_slider_value_changed_event_t event;
    bool changed = false;

    if(root->updating_low) {
        int32_t low_max = root->dual ? root->high : root->max;
        value = snap_value_to_step(root, value, root->min, low_max);
        int32_t old_value = root->dual ? root->low : root->value;

        if(root->dual) root->low = value;
        else root->value = value;
        lv_arc_set_value(root->low_dot_knob_arc, value);
        apply_touch_arc_value(root, value);

        changed = old_value != value;
        event.value = value;
        event.is_low = true;
    } else if(root->dual && root->updating_high) {
        value = snap_value_to_step(root, value, root->low, root->max);
        int32_t old_value = root->high;

        root->high = value;
        lv_arc_set_value(root->high_dot_knob_arc, value);
        apply_touch_arc_value(root, value);

        changed = old_value != value;
        event.value = value;
        event.is_low = false;
    }

    refresh_value_arcs(root);
    if(changed) lv_obj_send_event((lv_obj_t *)root, LV_EVENT_VALUE_CHANGED, &event);
}

static void pointer_finish(climate_control_circular_slider_t * root)
{
    if(root->updating_low || root->updating_high) {
        climate_control_circular_slider_value_changed_event_t event = {
            .value = root->dual ? (root->updating_low ? root->low : root->high)
                                : root->value,
            .is_low = root->updating_low,
        };
        lv_obj_send_event((lv_obj_t *)root, LV_EVENT_RELEASED, &event);
    }
    root->updating_low = false;
    root->updating_high = false;
}

static bool active_pointer_value(climate_control_circular_slider_t * root, int32_t * value)
{
    lv_indev_t * indev = lv_indev_active();
    if(indev == NULL || value == NULL) return false;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    *value = point_to_value(root, root->touch_arc, &point);
    return true;
}

static void side_hit_event_cb(lv_event_t * e)
{
    climate_control_circular_slider_t * root = lv_event_get_user_data(e);
    if(root == NULL) return;
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        pointer_finish(root);
        return;
    }
    if(!lv_obj_has_flag(root->touch_arc, LV_OBJ_FLAG_CLICKABLE)) return;
    int32_t value;
    if(code == LV_EVENT_PRESSED && active_pointer_value(root, &value)) {
        pointer_begin(root, value);
        pointer_update(root, value);
    } else if(code == LV_EVENT_PRESSING && active_pointer_value(root, &value)) {
        pointer_update(root, value);
    }
}

void touch_event_cb(lv_event_t * e)
{
    climate_control_circular_slider_t * root = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_PRESSED) {
        int32_t value = lv_arc_get_value(root->touch_arc);
        active_pointer_value(root, &value);
        pointer_begin(root, value);
    } else if(code == LV_EVENT_VALUE_CHANGED) {
        pointer_update(root, lv_arc_get_value(root->touch_arc));
    } else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        pointer_finish(root);
    }
}

void climate_control_circular_slider_constructor_hook(lv_obj_t * obj)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) obj;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    root->current = lv_arc_get_value(root->cur_dot_knob_arc);
    root->value = lv_arc_get_value(root->low_dot_knob_arc);
    root->low = root->value;
    root->high = lv_arc_get_value(root->high_dot_knob_arc);
    root->min = 0;
    root->max = 100;
    apply_value_arc_range(root);
    root->slider_step = 1;
    root->step = 1;
    lv_obj_add_event_cb(root->touch_arc, touch_event_cb, LV_EVENT_PRESSED,  root);
    lv_obj_add_event_cb(root->touch_arc, touch_event_cb, LV_EVENT_VALUE_CHANGED,  root);
    lv_obj_add_event_cb(root->touch_arc, touch_event_cb, LV_EVENT_RELEASED,  root);
    lv_obj_add_event_cb(root->touch_arc, touch_event_cb, LV_EVENT_PRESS_LOST,  root);
    lv_obj_remove_flag(root->touch_arc, LV_OBJ_FLAG_ADV_HITTEST);
    lv_arc_set_change_rate(root->touch_arc, 36000);
    climate_control_circular_slider_set_size(obj, CLIMATE_CIRCULAR_SLIDER_DEFAULT_SIZE);
    root->show_knob = true;
    root->dual = false;
    apply_dual_visibility(root);
}

void climate_control_circular_slider_destructor_hook(lv_obj_t * obj)
{

}

void climate_control_circular_slider_event_hook(lv_event_t * e)
{

}

int32_t climate_control_circular_slider_get_current(lv_obj_t * climate_control_circular_slider)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    return root->current;
}

int32_t climate_control_circular_slider_get_value(lv_obj_t * climate_control_circular_slider)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    return root->value;
}

int32_t climate_control_circular_slider_get_low(lv_obj_t * climate_control_circular_slider)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    return root->low;
}

int32_t climate_control_circular_slider_get_high(lv_obj_t * climate_control_circular_slider)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    return root->high;
}

bool climate_control_circular_slider_is_updating_high(lv_obj_t * climate_control_circular_slider)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    return root != NULL && root->updating_high;
}

bool climate_control_circular_slider_is_updating_low(lv_obj_t * climate_control_circular_slider)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    return root != NULL && root->updating_low;
}

void climate_control_circular_slider_set_current(lv_obj_t * climate_control_circular_slider, int32_t current)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    root->current = clamp_i32(current, root->min, root->max);
    lv_arc_set_value(root->cur_dot_knob_arc, root->current);
    refresh_value_arcs(root);
}

void climate_control_circular_slider_set_value(lv_obj_t * climate_control_circular_slider, int32_t value)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    root->value = clamp_i32(value, root->min, root->max);
    lv_arc_set_value(root->low_dot_knob_arc, root->value);
    refresh_value_arcs(root);
}

void climate_control_circular_slider_set_low(lv_obj_t * climate_control_circular_slider, int32_t low)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    root->low = clamp_i32(low, root->min, root->max);
    lv_arc_set_value(root->low_dot_knob_arc, root->low);
    refresh_value_arcs(root);
}

void climate_control_circular_slider_set_high(lv_obj_t * climate_control_circular_slider, int32_t high)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    root->high = clamp_i32(high, root->min, root->max);
    lv_arc_set_value(root->high_dot_knob_arc, root->high);
    refresh_value_arcs(root);
}

void climate_control_circular_slider_set_dual(lv_obj_t * climate_control_circular_slider, bool dual)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    root->dual = dual;
    apply_dual_visibility(root);
    refresh_value_arcs(root);
}

void climate_control_circular_slider_set_min(lv_obj_t * climate_control_circular_slider, int32_t min)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    root->min = min;
    climate_control_circular_slider_set_range(climate_control_circular_slider, min, root->max);
}

void climate_control_circular_slider_set_max(lv_obj_t * climate_control_circular_slider, int32_t max)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    root->max = max;
    climate_control_circular_slider_set_range(climate_control_circular_slider, root->min, max);
}

void climate_control_circular_slider_set_range(lv_obj_t * climate_control_circular_slider, int32_t min, int32_t max)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if(max < min) {
        int32_t tmp = min;
        min = max;
        max = tmp;
    }

    root->min = min;
    root->max = max;
    apply_value_arc_range(root);
    if(root->dual) {
        climate_control_circular_slider_set_low(climate_control_circular_slider, root->low);
        climate_control_circular_slider_set_high(climate_control_circular_slider, root->high);
    } else {
        climate_control_circular_slider_set_value(climate_control_circular_slider, root->value);
    }
}

void climate_control_circular_slider_set_step(lv_obj_t * climate_control_circular_slider, int32_t step)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    int32_t valid_step = step < 1 ? 1 : step;
    root->slider_step = valid_step;
    root->step = valid_step;
}

void climate_control_circular_slider_set_disabled(lv_obj_t * climate_control_circular_slider, bool disabled)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if(root == NULL || root->touch_arc == NULL) return;
    if(disabled) lv_obj_remove_flag(root->touch_arc, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_add_flag(root->touch_arc, LV_OBJ_FLAG_CLICKABLE);
}

void climate_control_circular_slider_set_touch_width(lv_obj_t * climate_control_circular_slider,
                                                     int32_t width)
{
    climate_control_circular_slider_t * root =
        (climate_control_circular_slider_t *) climate_control_circular_slider;
    if(root == NULL || root->touch_arc == NULL) return;

    width = LV_MAX(width, root->size);
    lv_obj_set_size(climate_control_circular_slider, width, root->size);
    lv_obj_set_size(root->touch_arc, root->size, root->size);
    lv_obj_center(root->touch_arc);

    int32_t margin = (width - root->size) / 2;
    int32_t overlap = scale_dim(root->size, CLIMATE_CIRCULAR_SLIDER_SIDE_OVERLAP);
    int32_t hit_width = margin + overlap;
    for(uint32_t i = 0; i < 2; ++i) {
        if(root->side_hit_areas[i] == NULL) {
            root->side_hit_areas[i] = lv_obj_create(climate_control_circular_slider);
            if(root->side_hit_areas[i] == NULL) continue;
            lv_obj_remove_style_all(root->side_hit_areas[i]);
            lv_obj_remove_flag(root->side_hit_areas[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(root->side_hit_areas[i],
                            LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_add_event_cb(root->side_hit_areas[i], side_hit_event_cb,
                                LV_EVENT_ALL, root);
        }
        lv_obj_set_pos(root->side_hit_areas[i],
                       i == 0 ? 0 : width - hit_width, 0);
        lv_obj_set_size(root->side_hit_areas[i], hit_width, root->size);
        lv_obj_remove_flag(root->side_hit_areas[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void climate_control_circular_slider_set_size(lv_obj_t * climate_control_circular_slider, int32_t size)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if(size < CLIMATE_CIRCULAR_SLIDER_MIN_SIZE) size = CLIMATE_CIRCULAR_SLIDER_MIN_SIZE;

    root->size = size;

    for(uint32_t i = 0; i < 2; ++i) {
        if(root->side_hit_areas[i])
            lv_obj_add_flag(root->side_hit_areas[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_size(climate_control_circular_slider, size, size);

    int32_t line_width = scale_dim(size, CLIMATE_CIRCULAR_SLIDER_LINE_WIDTH);
    int32_t knob_size = scale_dim(size, CLIMATE_CIRCULAR_SLIDER_KNOB_SIZE);
    int32_t touch_width = line_width +
                          scale_dim(size, CLIMATE_CIRCULAR_SLIDER_TOUCH_EXTRA);
    int32_t ext_click = scale_dim(size, CLIMATE_CIRCULAR_SLIDER_EXT_CLICK);

    lv_obj_t * arcs[] = {
        root->bg_arc,
        root->low_arc,
        root->low_active_arc,
        root->high_arc,
        root->high_active_arc,
        root->cur_dot_knob_arc,
        root->low_dot_knob_arc,
        root->high_dot_knob_arc,
        root->touch_arc,
    };

    for(uint32_t i = 0; i < sizeof(arcs) / sizeof(arcs[0]); i++) {
        if(arcs[i] == NULL) continue;
        lv_obj_set_size(arcs[i], size, size);
        lv_obj_center(arcs[i]);
        int32_t arc_width = arcs[i] == root->touch_arc ? touch_width : line_width;
        lv_obj_set_style_arc_width(arcs[i], arc_width, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arcs[i], arc_width, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arcs[i], true, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arcs[i], true, LV_PART_INDICATOR);
    }

    lv_obj_set_style_width(root->cur_dot_knob_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_style_height(root->cur_dot_knob_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_style_width(root->low_dot_knob_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_style_height(root->low_dot_knob_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_style_width(root->high_dot_knob_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_style_height(root->high_dot_knob_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_style_width(root->touch_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_style_height(root->touch_arc, knob_size, LV_PART_KNOB);
    lv_obj_set_ext_click_area(root->touch_arc, ext_click);
}


void climate_control_circular_slider_set_low_color(lv_obj_t * climate_control_circular_slider, lv_color_t color)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if (root != NULL) {
        if (root->low_arc != NULL) {
            lv_obj_set_style_arc_color(root->low_arc, color, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(root->low_arc, (255 * 50 / 100), LV_PART_INDICATOR);
        }
        if (root->low_active_arc != NULL) {
            lv_obj_set_style_arc_color(root->low_active_arc, color, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(root->low_active_arc, LV_OPA_COVER, LV_PART_INDICATOR);
        }
    }
}

void climate_control_circular_slider_set_high_color(lv_obj_t * climate_control_circular_slider, lv_color_t color)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if (root != NULL) {
        if (root->high_arc != NULL) {
            lv_obj_set_style_arc_color(root->high_arc, color, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(root->high_arc, (255 * 50 / 100), LV_PART_INDICATOR);
        }
        if (root->high_active_arc != NULL) {
            lv_obj_set_style_arc_color(root->high_active_arc, color, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(root->high_active_arc, LV_OPA_COVER, LV_PART_INDICATOR);
        }
    }
}

void climate_control_circular_slider_set_active_color(lv_obj_t * climate_control_circular_slider, lv_color_t color)
{
    climate_control_circular_slider_set_low_color(climate_control_circular_slider, color);
}

void climate_control_circular_slider_set_inactive(lv_obj_t * climate_control_circular_slider, bool inactive)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if (root != NULL) {
        if (inactive) {
            if (root->low_arc) lv_obj_set_style_arc_opa(root->low_arc, (255 * 20 / 100), LV_PART_INDICATOR);
            if (root->low_active_arc) lv_obj_set_style_arc_opa(root->low_active_arc, (255 * 20 / 100), LV_PART_INDICATOR);
            if (root->high_arc) lv_obj_set_style_arc_opa(root->high_arc, (255 * 20 / 100), LV_PART_INDICATOR);
            if (root->high_active_arc) lv_obj_set_style_arc_opa(root->high_active_arc, (255 * 20 / 100), LV_PART_INDICATOR);
        } else {
            if (root->low_arc) lv_obj_set_style_arc_opa(root->low_arc, (255 * 50 / 100), LV_PART_INDICATOR);
            if (root->low_active_arc) lv_obj_set_style_arc_opa(root->low_active_arc, LV_OPA_COVER, LV_PART_INDICATOR);
            if (root->high_arc) lv_obj_set_style_arc_opa(root->high_arc, (255 * 50 / 100), LV_PART_INDICATOR);
            if (root->high_active_arc) lv_obj_set_style_arc_opa(root->high_active_arc, LV_OPA_COVER, LV_PART_INDICATOR);
        }
    }
}

void climate_control_circular_slider_set_mode(lv_obj_t * climate_control_circular_slider, climate_circular_slider_mode_t mode)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if (root != NULL) {
        root->mode = mode;
        refresh_value_arcs(root);
    }
}

void climate_control_circular_slider_set_show_knob(lv_obj_t * climate_control_circular_slider, bool show_knob)
{
    climate_control_circular_slider_t * root = (climate_control_circular_slider_t *) climate_control_circular_slider;
    if (root != NULL) {
        root->show_knob = show_knob;
        if (show_knob) {
            if (root->cur_dot_knob_arc) lv_obj_remove_flag(root->cur_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
            if (root->low_dot_knob_arc) lv_obj_remove_flag(root->low_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
            if (root->dual && root->high_dot_knob_arc) lv_obj_remove_flag(root->high_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (root->cur_dot_knob_arc) lv_obj_add_flag(root->cur_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
            if (root->low_dot_knob_arc) lv_obj_add_flag(root->low_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
            if (root->high_dot_knob_arc) lv_obj_add_flag(root->high_dot_knob_arc, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
