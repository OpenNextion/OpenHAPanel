/**
 * @file ha_control_select_gen.h
 *
 */

#ifndef HA_CONTROL_SELECT_GEN_H
#define HA_CONTROL_SELECT_GEN_H

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
 * Create a ha_control_select object
 * @param parent pointer to an object, it will be the parent of the new ha_control_select
 * @return pointer to the created ha_control_select
 */
lv_obj_t * ha_control_select_create(lv_obj_t * parent);

/**
 * Selected value
 * @param obj   pointer to a ha_control_select
 * @param value  Selected value
 */
void ha_control_select_set_value(lv_obj_t * ha_control_select, const char * value);

/**
 * Semicolon-separated options (value,label,symbol,color or just values)
 * @param obj   pointer to a ha_control_select
 * @param options  Semicolon-separated options (value,label,symbol,color or just values)
 */
void ha_control_select_set_options(lv_obj_t * ha_control_select, const char * options);

/**
 * Control label
 * @param obj   pointer to a ha_control_select
 * @param label  Control label
 */
void ha_control_select_set_label(lv_obj_t * ha_control_select, const char * label);

/**
 * Disabled state
 * @param obj   pointer to a ha_control_select
 * @param disabled  Disabled state
 */
void ha_control_select_set_disabled(lv_obj_t * ha_control_select, bool disabled);

/**
 * Vertical layout orientation
 * @param obj   pointer to a ha_control_select
 * @param vertical  Vertical layout orientation
 */
void ha_control_select_set_vertical(lv_obj_t * ha_control_select, bool vertical);

/**
 * Hide text labels in options
 * @param obj   pointer to a ha_control_select
 * @param hide_option_label  Hide text labels in options
 */
void ha_control_select_set_hide_option_label(lv_obj_t * ha_control_select, bool hide_option_label);

/**
 * Selected option highlight color
 * @param obj   pointer to a ha_control_select
 * @param color  Selected option highlight color
 */
void ha_control_select_set_color(lv_obj_t * ha_control_select, lv_color_t color);

/**
 * Height of the control
 * @param obj   pointer to a ha_control_select
 * @param height  Height of the control
 */
void ha_control_select_set_height(lv_obj_t * ha_control_select, lv_coord_t height);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*HA_CONTROL_SELECT_GEN_H*/
