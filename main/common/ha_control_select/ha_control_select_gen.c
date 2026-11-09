/**
 * @file ha_control_select_gen.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "ha_control_select_private_gen.h"
#include <lvgl.h>
#include <lvgl_private.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

void ha_control_select_constructor_hook(lv_obj_t * obj);
void ha_control_select_destructor_hook(lv_obj_t * obj);
void ha_control_select_event_hook(lv_event_t * e);

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void ha_control_select_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void ha_control_select_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void ha_control_select_event(const lv_obj_class_t * class_p, lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t ha_control_select_class = {
    .base_class = &lv_buttonmatrix_class,
    .constructor_cb = ha_control_select_constructor,
    .destructor_cb = ha_control_select_destructor,
    .event_cb = ha_control_select_event,
    .instance_size = sizeof(ha_control_select_t),
    .editable = 1,
    .name = "ha_control_select"
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * ha_control_select_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&ha_control_select_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void ha_control_select_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    ha_control_select_t * widget = (ha_control_select_t *)obj;
    static lv_style_t style_base;
    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_base);

        lv_style_set_width(&style_base, lv_pct(100));
        lv_style_set_height(&style_base, 44);
        lv_style_set_radius(&style_base, 12);
        lv_style_set_bg_color(&style_base, lv_color_hex(0x727272));
        lv_style_set_bg_opa(&style_base, (255 * 20 / 100));
        lv_style_set_pad_all(&style_base, 4);
        lv_style_set_pad_column(&style_base, 4);
        lv_style_set_pad_row(&style_base, 4);
        lv_style_set_border_width(&style_base, 0);

        style_inited = true;
    }

    lv_obj_set_flag(obj, LV_OBJ_FLAG_SCROLLABLE, false);

    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &style_base, 0);

    ha_control_select_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void ha_control_select_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    ha_control_select_destructor_hook(obj);
}

static void ha_control_select_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    /* Call the ancestor's event handler */
    res = lv_obj_event_base(&ha_control_select_class, e);
    if(res != LV_RESULT_OK) return;

    ha_control_select_event_hook(e);
}
