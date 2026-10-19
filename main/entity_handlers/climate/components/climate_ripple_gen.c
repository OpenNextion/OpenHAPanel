/**
 * @file climate_ripple_gen.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_ripple_private_gen.h"
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

void climate_ripple_constructor_hook(lv_obj_t * obj);
void climate_ripple_destructor_hook(lv_obj_t * obj);
void climate_ripple_event_hook(lv_event_t * e);

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void climate_ripple_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_ripple_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_ripple_event(const lv_obj_class_t * class_p, lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t climate_ripple_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = climate_ripple_constructor,
    .destructor_cb = climate_ripple_destructor,
    .event_cb = climate_ripple_event,
    .instance_size = sizeof(climate_ripple_t),
    .editable = 1,
    .name = "climate_ripple"
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * climate_ripple_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&climate_ripple_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void climate_ripple_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_base;
    static bool style_inited = false;

    if (!style_inited) {
        lv_style_init(&style_base);
        lv_style_set_bg_opa(&style_base, (255 * 0 / 100));
        lv_style_set_border_width(&style_base, 0);
        lv_style_set_radius(&style_base, 8);
        style_inited = true;
    }

    lv_obj_set_flag(obj, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_flag(obj, LV_OBJ_FLAG_FLOATING, true);
    lv_obj_set_flag(obj, LV_OBJ_FLAG_IGNORE_LAYOUT, true);

    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &style_base, 0);

    climate_ripple_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void climate_ripple_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    climate_ripple_destructor_hook(obj);
}

static void climate_ripple_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    /* Call the ancestor's event handler */
    res = lv_obj_event_base(&climate_ripple_class, e);
    if(res != LV_RESULT_OK) return;

    climate_ripple_event_hook(e);
}
