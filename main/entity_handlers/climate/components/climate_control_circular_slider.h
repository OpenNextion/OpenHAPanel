/**
 * @file climate_control_circular_slider.h
 *
 */

#ifndef CLIMATE_CONTROL_CIRCULAR_SLIDER_H
#define CLIMATE_CONTROL_CIRCULAR_SLIDER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "climate_control_circular_slider_gen.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    int32_t value;
    bool is_low;
} climate_control_circular_slider_value_changed_event_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
int32_t climate_control_circular_slider_get_current(lv_obj_t * climate_control_circular_slider);
int32_t climate_control_circular_slider_get_value(lv_obj_t * climate_control_circular_slider);
int32_t climate_control_circular_slider_get_low(lv_obj_t * climate_control_circular_slider);
int32_t climate_control_circular_slider_get_high(lv_obj_t * climate_control_circular_slider);
bool climate_control_circular_slider_is_updating_high(lv_obj_t * climate_control_circular_slider);
bool climate_control_circular_slider_is_updating_low(lv_obj_t * climate_control_circular_slider);
void climate_control_circular_slider_set_range(lv_obj_t * climate_control_circular_slider, int32_t min, int32_t max);
void climate_control_circular_slider_set_disabled(lv_obj_t * climate_control_circular_slider, bool disabled);
void climate_control_circular_slider_set_touch_width(lv_obj_t * climate_control_circular_slider,
                                                     int32_t width);
void climate_control_circular_slider_set_background_track(
    lv_obj_t * climate_control_circular_slider,
    lv_color_t color,
    lv_opa_t opacity);

/**********************
 *      MACROS
 **********************/

#define CLIMATE_CONTROL_CIRCULAR_SLIDER_USER_DATA \
    int32_t slider_step; \
    bool updating_low; \
    bool updating_high; \
    lv_obj_t * side_hit_areas[2];

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_CONTROL_CIRCULAR_SLIDER_H*/
