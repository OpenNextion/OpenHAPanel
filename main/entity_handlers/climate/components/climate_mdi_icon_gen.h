/**
 * @file climate_mdi_icon_gen.h
 *
 */

#ifndef CLIMATE_MDI_ICON_GEN_H
#define CLIMATE_MDI_ICON_GEN_H

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
 * Create a climate_mdi_icon object
 * @param parent pointer to an object, it will be the parent of the new climate_mdi_icon
 * @return pointer to the created climate_mdi_icon
 */
lv_obj_t * climate_mdi_icon_create(lv_obj_t * parent);

/**
 * climate_mdi_icon size
 * @param obj   pointer to a climate_mdi_icon
 * @param size  size
 */
void climate_mdi_icon_set_size(lv_obj_t * climate_mdi_icon, int32_t size);

/**
 * climate_mdi_icon symbol
 * @param obj   pointer to a climate_mdi_icon
 * @param symbol  symbol
 */
void climate_mdi_icon_set_symbol(lv_obj_t * climate_mdi_icon, const char * symbol);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_MDI_ICON_GEN_H*/
