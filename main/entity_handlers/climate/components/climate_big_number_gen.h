/**
 * @file climate_big_number_gen.h
 *
 */

#ifndef CLIMATE_BIG_NUMBER_GEN_H
#define CLIMATE_BIG_NUMBER_GEN_H

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
typedef enum {
    UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_TOP = 0,
    UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_BOTTOM = 1
}climate_big_number_unit_position_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a climate_big_number object
 * @param parent pointer to an object, it will be the parent of the new climate_big_number
 * @return pointer to the created climate_big_number
 */
lv_obj_t * climate_big_number_create(lv_obj_t * parent);
/**
 * climate_big_number value
 * @param obj   pointer to a climate_big_number
 * @param value  value
 */
void climate_big_number_set_value(lv_obj_t * climate_big_number, int32_t value);

/**
 * climate_big_number decimal_places
 * @param obj   pointer to a climate_big_number
 * @param decimal_places  decimal_places
 */
void climate_big_number_set_decimal_places(lv_obj_t * climate_big_number, int32_t decimal_places);

/**
 * climate_big_number unit
 * @param obj   pointer to a climate_big_number
 * @param unit  unit
 */
void climate_big_number_set_unit(lv_obj_t * climate_big_number, const char * unit);

/**
 * climate_big_number unit_position
 * @param obj   pointer to a climate_big_number
 * @param unit_position  unit_position
 */
void climate_big_number_set_unit_position(lv_obj_t * climate_big_number, climate_big_number_unit_position_t unit_position);

/**
 * climate_big_number text_color
 * @param obj   pointer to a climate_big_number
 * @param text_color  text_color
 */
void climate_big_number_set_text_color(lv_obj_t * climate_big_number, lv_color_t text_color);

/**
 * climate_big_number integer_label_text_font
 * @param obj   pointer to a climate_big_number
 * @param integer_label_text_font  integer_label_text_font
 */
void climate_big_number_set_integer_label_text_font(lv_obj_t * climate_big_number, const lv_font_t * integer_label_text_font);

/**
 * climate_big_number decimal_label_text_font
 * @param obj   pointer to a climate_big_number
 * @param decimal_label_text_font  decimal_label_text_font
 */
void climate_big_number_set_decimal_label_text_font(lv_obj_t * climate_big_number, const lv_font_t * decimal_label_text_font);

/**
 * climate_big_number unit_label_text_font
 * @param obj   pointer to a climate_big_number
 * @param unit_label_text_font  unit_label_text_font
 */
void climate_big_number_set_unit_label_text_font(lv_obj_t * climate_big_number, const lv_font_t * unit_label_text_font);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLIMATE_BIG_NUMBER_GEN_H*/
