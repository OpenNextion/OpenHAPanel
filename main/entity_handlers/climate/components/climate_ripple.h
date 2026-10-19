/**
 * @file climate_ripple.h
 *
 */

#ifndef CLIMATE_RIPPLE_H
#define CLIMATE_RIPPLE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "climate_ripple_gen.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void climate_ripple_attach(lv_obj_t * obj, lv_obj_t * control);
void climate_ripple_detach(lv_obj_t * obj);

bool climate_ripple_get_disabled(lv_obj_t * obj);

/**********************
 *      MACROS
 **********************/
#define CLIMATE_RIPPLE_USER_DATA \
    lv_obj_t * control;     \
    lv_color_t color;       \
    uint8_t hover_opacity;  \
    uint8_t pressed_opacity;\
    int32_t radius;         \
    bool disabled;          \
    int32_t ripple_x;       \
    int32_t ripple_y;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_RIPPLE_H*/
