/**
 * @file climate_outlined_icon_button.h
 *
 */

#ifndef CLIMATE_OUTLINED_ICON_BUTTON_H
#define CLIMATE_OUTLINED_ICON_BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "climate_outlined_icon_button_gen.h"
#include "assets/mdi_symbol_def.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void climate_outlined_icon_button_set_text_font(lv_obj_t * obj, const lv_font_t * font);
int32_t climate_outlined_icon_button_get_size(lv_obj_t * obj);
int32_t climate_outlined_icon_button_get_icon_size(lv_obj_t * obj);
const char * climate_outlined_icon_button_get_symbol(lv_obj_t * obj);
lv_color_t climate_outlined_icon_button_get_outline_color(lv_obj_t * obj);
lv_color_t climate_outlined_icon_button_get_text_color(lv_obj_t * obj);
bool climate_outlined_icon_button_get_disabled(lv_obj_t * obj);
bool climate_outlined_icon_button_get_no_ripple(lv_obj_t * obj);

/**********************
 *      MACROS
 **********************/

#define CLIMATE_OUTLINED_ICON_BUTTON_USER_DATA

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_OUTLINED_ICON_BUTTON_H*/
