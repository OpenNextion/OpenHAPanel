/**
 * @file climate_mdi_icon.h
 *
 */

#ifndef CLIMATE_MDI_ICON_H
#define CLIMATE_MDI_ICON_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "climate_mdi_icon_gen.h"
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

void climate_mdi_icon_set_text_font(lv_obj_t * climate_mdi_icon, const lv_font_t * font);
void climate_mdi_icon_set_text_color(lv_obj_t * climate_mdi_icon, lv_color_t text_color);

/**********************
 *      MACROS
 **********************/

#define CLIMATE_MDI_ICON_USER_DATA

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_MDI_ICON_H*/
