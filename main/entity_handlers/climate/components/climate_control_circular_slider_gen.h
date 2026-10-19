/**
 * @file climate_control_circular_slider_gen.h
 *
 */

#ifndef CLIMATE_CONTROL_CIRCULAR_SLIDER_GEN_H
#define CLIMATE_CONTROL_CIRCULAR_SLIDER_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl.h>
#include <lvgl_private.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    CLIMATE_CIRCULAR_SLIDER_MODE_START = 0,
    CLIMATE_CIRCULAR_SLIDER_MODE_END = 1,
    CLIMATE_CIRCULAR_SLIDER_MODE_FULL = 2,
    CLIMATE_CIRCULAR_SLIDER_MODE_NONE = 3
}climate_circular_slider_mode_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a climate_control_circular_slider object
 * @param parent pointer to an object, it will be the parent of the new climate_control_circular_slider
 * @return pointer to the created climate_control_circular_slider
 */
lv_obj_t * climate_control_circular_slider_create(lv_obj_t * parent);
/**
 * climate_control_circular_slider current
 * @param obj   pointer to a climate_control_circular_slider
 * @param current  current
 */
void climate_control_circular_slider_set_current(lv_obj_t * climate_control_circular_slider, int32_t current);

/**
 * climate_control_circular_slider value
 * @param obj   pointer to a climate_control_circular_slider
 * @param value  value
 */
void climate_control_circular_slider_set_value(lv_obj_t * climate_control_circular_slider, int32_t value);

/**
 * climate_control_circular_slider low
 * @param obj   pointer to a climate_control_circular_slider
 * @param low  low
 */
void climate_control_circular_slider_set_low(lv_obj_t * climate_control_circular_slider, int32_t low);

/**
 * climate_control_circular_slider high
 * @param obj   pointer to a climate_control_circular_slider
 * @param high  high
 */
void climate_control_circular_slider_set_high(lv_obj_t * climate_control_circular_slider, int32_t high);

/**
 * climate_control_circular_slider dual
 * @param obj   pointer to a climate_control_circular_slider
 * @param dual  dual
 */
void climate_control_circular_slider_set_dual(lv_obj_t * climate_control_circular_slider, bool dual);

/**
 * climate_control_circular_slider min
 * @param obj   pointer to a climate_control_circular_slider
 * @param min  min
 */
void climate_control_circular_slider_set_min(lv_obj_t * climate_control_circular_slider, int32_t min);

/**
 * climate_control_circular_slider max
 * @param obj   pointer to a climate_control_circular_slider
 * @param max  max
 */
void climate_control_circular_slider_set_max(lv_obj_t * climate_control_circular_slider, int32_t max);

/**
 * climate_control_circular_slider step
 * @param obj   pointer to a climate_control_circular_slider
 * @param step  step
 */
void climate_control_circular_slider_set_step(lv_obj_t * climate_control_circular_slider, int32_t step);

/**
 * climate_control_circular_slider size
 * @param obj   pointer to a climate_control_circular_slider
 * @param size  size
 */
void climate_control_circular_slider_set_size(lv_obj_t * climate_control_circular_slider, int32_t size);

/**
 * climate_control_circular_slider low_color
 * @param obj   pointer to a climate_control_circular_slider
 * @param low_color  low_color
 */
void climate_control_circular_slider_set_low_color(lv_obj_t * climate_control_circular_slider, lv_color_t low_color);

/**
 * climate_control_circular_slider high_color
 * @param obj   pointer to a climate_control_circular_slider
 * @param high_color  high_color
 */
void climate_control_circular_slider_set_high_color(lv_obj_t * climate_control_circular_slider, lv_color_t high_color);

/**
 * climate_control_circular_slider active_color
 * @param obj   pointer to a climate_control_circular_slider
 * @param active_color  active_color
 */
void climate_control_circular_slider_set_active_color(lv_obj_t * climate_control_circular_slider, lv_color_t active_color);

/**
 * climate_control_circular_slider mode
 * @param obj   pointer to a climate_control_circular_slider
 * @param mode  mode
 */
void climate_control_circular_slider_set_mode(lv_obj_t * climate_control_circular_slider, climate_circular_slider_mode_t mode);

/**
 * climate_control_circular_slider inactive
 * @param obj   pointer to a climate_control_circular_slider
 * @param inactive  inactive
 */
void climate_control_circular_slider_set_inactive(lv_obj_t * climate_control_circular_slider, bool inactive);

/**
 * climate_control_circular_slider show_knob
 * @param obj   pointer to a climate_control_circular_slider
 * @param show_knob  show_knob
 */
void climate_control_circular_slider_set_show_knob(lv_obj_t * climate_control_circular_slider, bool show_knob);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_CONTROL_CIRCULAR_SLIDER_GEN_H*/
