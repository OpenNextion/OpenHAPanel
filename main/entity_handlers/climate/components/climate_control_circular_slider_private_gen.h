/**
 * @file climate_control_circular_slider_private_gen.h
 *
 */

#ifndef CLIMATE_CONTROL_CIRCULAR_SLIDER_PRIVATE_H
#define CLIMATE_CONTROL_CIRCULAR_SLIDER_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl_private.h>
#include "climate_control_circular_slider.h"

/*********************
 *      DEFINES
 *********************/

#ifndef CLIMATE_CONTROL_CIRCULAR_SLIDER_USER_DATA
#define CLIMATE_CONTROL_CIRCULAR_SLIDER_USER_DATA
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t obj;  /* Base widget to extend */
    int32_t current;
    int32_t value;
    int32_t low;
    int32_t high;
    bool dual;
    int32_t min;
    int32_t max;
    int32_t step;
    int32_t size;
    lv_color_t low_color;
    lv_color_t high_color;
    lv_color_t active_color;
    climate_circular_slider_mode_t mode;
    bool inactive;
    bool show_knob;
    lv_obj_t * bg_arc;
    lv_obj_t * low_arc;
    lv_obj_t * low_active_arc;
    lv_obj_t * high_arc;
    lv_obj_t * high_active_arc;
    lv_obj_t * cur_dot_knob_arc;
    lv_obj_t * low_dot_knob_arc;
    lv_obj_t * high_dot_knob_arc;
    lv_obj_t * touch_arc;
    CLIMATE_CONTROL_CIRCULAR_SLIDER_USER_DATA
} climate_control_circular_slider_t;

extern const lv_obj_class_t climate_control_circular_slider_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#if defined(LV_USE_XML) && LV_USE_XML
    void climate_control_circular_slider_register(void);
#endif

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_CONTROL_CIRCULAR_SLIDER_PRIVATE_H*/
