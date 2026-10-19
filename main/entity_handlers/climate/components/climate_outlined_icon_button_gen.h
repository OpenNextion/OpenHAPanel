/**
 * @file climate_outlined_icon_button_gen.h
 *
 */

#ifndef CLIMATE_OUTLINED_ICON_BUTTON_GEN_H
#define CLIMATE_OUTLINED_ICON_BUTTON_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "climate_ripple.h"
#include "climate_mdi_icon.h"

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
 * Create a climate_outlined_icon_button object
 * @param parent pointer to an object, it will be the parent of the new climate_outlined_icon_button
 * @return pointer to the created climate_outlined_icon_button
 */
lv_obj_t * climate_outlined_icon_button_create(lv_obj_t * parent);

/**
 * Button diameter size
 * @param obj   pointer to a climate_outlined_icon_button
 * @param size  Button diameter size
 */
void climate_outlined_icon_button_set_size(lv_obj_t * climate_outlined_icon_button, int32_t size);

/**
 * Icon font size
 * @param obj   pointer to a climate_outlined_icon_button
 * @param icon_size  Icon font size
 */
void climate_outlined_icon_button_set_icon_size(lv_obj_t * climate_outlined_icon_button, int32_t icon_size);

/**
 * Icon symbol
 * @param obj   pointer to a climate_outlined_icon_button
 * @param symbol  Icon symbol
 */
void climate_outlined_icon_button_set_symbol(lv_obj_t * climate_outlined_icon_button, const char * symbol);

/**
 * Outline border color
 * @param obj   pointer to a climate_outlined_icon_button
 * @param outline_color  Outline border color
 */
void climate_outlined_icon_button_set_outline_color(lv_obj_t * climate_outlined_icon_button, lv_color_t outline_color);

/**
 * Icon text color
 * @param obj   pointer to a climate_outlined_icon_button
 * @param text_color  Icon text color
 */
void climate_outlined_icon_button_set_text_color(lv_obj_t * climate_outlined_icon_button, lv_color_t text_color);

/**
 * Disabled state
 * @param obj   pointer to a climate_outlined_icon_button
 * @param disabled  Disabled state
 */
void climate_outlined_icon_button_set_disabled(lv_obj_t * climate_outlined_icon_button, bool disabled);

/**
 * Disable ripple effect
 * @param obj   pointer to a climate_outlined_icon_button
 * @param no_ripple  Disable ripple effect
 */
void climate_outlined_icon_button_set_no_ripple(lv_obj_t * climate_outlined_icon_button, bool no_ripple);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_OUTLINED_ICON_BUTTON_GEN_H*/
