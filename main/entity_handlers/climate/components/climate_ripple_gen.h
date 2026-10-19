/**
 * @file climate_ripple_gen.h
 *
 */

#ifndef CLIMATE_RIPPLE_GEN_H
#define CLIMATE_RIPPLE_GEN_H

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

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a climate_ripple object
 * @param parent pointer to an object, it will be the parent of the new climate_ripple
 * @return pointer to the created climate_ripple
 */
lv_obj_t * climate_ripple_create(lv_obj_t * parent);

/**
 * climate_ripple p_color
 * @param obj   pointer to a climate_ripple
 * @param p_color  p_color
 */
void climate_ripple_set_p_color(lv_obj_t * climate_ripple, lv_color_t p_color);

/**
 * climate_ripple p_hover_opacity
 * @param obj   pointer to a climate_ripple
 * @param p_hover_opacity  p_hover_opacity
 */
void climate_ripple_set_p_hover_opacity(lv_obj_t * climate_ripple, int32_t p_hover_opacity);

/**
 * climate_ripple p_pressed_opacity
 * @param obj   pointer to a climate_ripple
 * @param p_pressed_opacity  p_pressed_opacity
 */
void climate_ripple_set_p_pressed_opacity(lv_obj_t * climate_ripple, int32_t p_pressed_opacity);

/**
 * climate_ripple p_radius
 * @param obj   pointer to a climate_ripple
 * @param p_radius  p_radius
 */
void climate_ripple_set_p_radius(lv_obj_t * climate_ripple, int32_t p_radius);

/**
 * climate_ripple p_disabled
 * @param obj   pointer to a climate_ripple
 * @param p_disabled  p_disabled
 */
void climate_ripple_set_p_disabled(lv_obj_t * climate_ripple, bool p_disabled);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_RIPPLE_GEN_H*/
