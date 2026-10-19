/**
 * @file climate_big_number_private_gen.h
 *
 */

#ifndef CLIMATE_BIG_NUMBER_PRIVATE_H
#define CLIMATE_BIG_NUMBER_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl_private.h>
#include "climate_big_number.h"

/*********************
 *      DEFINES
 *********************/

#ifndef CLIMATE_BIG_NUMBER_USER_DATA
#define CLIMATE_BIG_NUMBER_USER_DATA
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t obj;  /* Base widget to extend */
    int32_t value;
    int32_t decimal_places;
    const char * unit;
    climate_big_number_unit_position_t unit_position;
    lv_color_t text_color;
    lv_obj_t * integer_label;
    lv_obj_t * addon;
    lv_obj_t * decimal_label;
    lv_obj_t * unit_label;
    CLIMATE_BIG_NUMBER_USER_DATA
} climate_big_number_t;

extern const lv_obj_class_t climate_big_number_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#if LV_USE_XML
    void climate_big_number_register(void);
#endif

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_BIG_NUMBER_PRIVATE_H*/
