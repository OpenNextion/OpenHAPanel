/**
 * @file climate_mdi_icon_gen.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_mdi_icon_private_gen.h"
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

void climate_mdi_icon_constructor_hook(lv_obj_t * obj);
void climate_mdi_icon_destructor_hook(lv_obj_t * obj);
void climate_mdi_icon_event_hook(lv_event_t * e);

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void climate_mdi_icon_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_mdi_icon_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_mdi_icon_event(const lv_obj_class_t * class_p, lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t climate_mdi_icon_class = {
    .base_class = &lv_label_class,
    .constructor_cb = climate_mdi_icon_constructor,
    .destructor_cb = climate_mdi_icon_destructor,
    .event_cb = climate_mdi_icon_event,
    .instance_size = sizeof(climate_mdi_icon_t),
    .editable = 1,
    .name = "climate_mdi_icon"
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * climate_mdi_icon_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&climate_mdi_icon_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void climate_mdi_icon_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lv_label_set_text(obj, "");
    lv_obj_set_flag(obj, LV_OBJ_FLAG_CLICKABLE, false);
    climate_mdi_icon_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void climate_mdi_icon_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    climate_mdi_icon_destructor_hook(obj);
}

static void climate_mdi_icon_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    /* Call the ancestor's event handler */
    res = lv_obj_event_base(&climate_mdi_icon_class, e);
    if(res != LV_RESULT_OK) return;

    climate_mdi_icon_event_hook(e);
}
