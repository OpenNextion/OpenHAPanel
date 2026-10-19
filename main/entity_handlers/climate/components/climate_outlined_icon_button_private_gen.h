/**
 * @file climate_outlined_icon_button_private_gen.h
 *
 */

#ifndef CLIMATE_OUTLINED_ICON_BUTTON_PRIVATE_H
#define CLIMATE_OUTLINED_ICON_BUTTON_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl_private.h>
#include "climate_outlined_icon_button.h"

/*********************
 *      DEFINES
 *********************/

#ifndef CLIMATE_OUTLINED_ICON_BUTTON_USER_DATA
#define CLIMATE_OUTLINED_ICON_BUTTON_USER_DATA
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t obj;  /* Base widget to extend */
    int32_t size;
    int32_t icon_size;
    const char * symbol;
    lv_color_t outline_color;
    lv_color_t text_color;
    bool disabled;
    bool no_ripple;
    lv_obj_t * ripple;
    lv_obj_t * icon;
    CLIMATE_OUTLINED_ICON_BUTTON_USER_DATA
} climate_outlined_icon_button_t;

extern const lv_obj_class_t climate_outlined_icon_button_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_OUTLINED_ICON_BUTTON_PRIVATE_H*/
