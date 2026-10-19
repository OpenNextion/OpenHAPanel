/**
 * @file climate_mdi_icon_private_gen.h
 *
 */

#ifndef CLIMATE_MDI_ICON_PRIVATE_H
#define CLIMATE_MDI_ICON_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl_private.h>
#include "climate_mdi_icon.h"

/*********************
 *      DEFINES
 *********************/

#ifndef CLIMATE_MDI_ICON_USER_DATA
#define CLIMATE_MDI_ICON_USER_DATA
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_label_t obj;  /* Base widget to extend */
    int32_t size;
    const char * symbol;
    CLIMATE_MDI_ICON_USER_DATA
} climate_mdi_icon_t;

extern const lv_obj_class_t climate_mdi_icon_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_MDI_ICON_PRIVATE_H*/
