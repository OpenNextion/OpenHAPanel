/**
 * @file ha_control_select_private_gen.h
 *
 */

#ifndef HA_CONTROL_SELECT_PRIVATE_H
#define HA_CONTROL_SELECT_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl_private.h>
#include "ha_control_select.h"

/*********************
 *      DEFINES
 *********************/

#ifndef HA_CONTROL_SELECT_USER_DATA
#define HA_CONTROL_SELECT_USER_DATA
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_buttonmatrix_t obj;  /* Base widget to extend */
    const char * value;
    const char * options;
    const char * label;
    bool disabled;
    bool vertical;
    bool hide_option_label;
    lv_color_t color;
    lv_coord_t height;
    HA_CONTROL_SELECT_USER_DATA
} ha_control_select_t;

extern const lv_obj_class_t ha_control_select_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#if defined(LV_USE_XML) && LV_USE_XML
void ha_control_select_register(void);
#endif

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*HA_CONTROL_SELECT_PRIVATE_H*/
