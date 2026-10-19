/**
 * @file climate_ripple_private_gen.h
 *
 */

#ifndef CLIMATE_RIPPLE_PRIVATE_H
#define CLIMATE_RIPPLE_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl_private.h>
#include "climate_ripple.h"

/*********************
 *      DEFINES
 *********************/

#ifndef CLIMATE_RIPPLE_USER_DATA
#define CLIMATE_RIPPLE_USER_DATA
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t obj;  /* Base widget to extend */
    lv_color_t p_color;
    int32_t p_hover_opacity;
    int32_t p_pressed_opacity;
    int32_t p_radius;
    bool p_disabled;
    CLIMATE_RIPPLE_USER_DATA
} climate_ripple_t;

extern const lv_obj_class_t climate_ripple_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_RIPPLE_PRIVATE_H*/
