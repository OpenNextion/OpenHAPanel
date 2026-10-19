/**
 * @file climate_big_number_gen.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_big_number_private_gen.h"
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

void climate_big_number_constructor_hook(lv_obj_t * obj);
void climate_big_number_destructor_hook(lv_obj_t * obj);
void climate_big_number_event_hook(lv_event_t * e);

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void climate_big_number_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_big_number_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_big_number_event(const lv_obj_class_t * class_p, lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t climate_big_number_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = climate_big_number_constructor,
    .destructor_cb = climate_big_number_destructor,
    .event_cb = climate_big_number_event,
    .instance_size = sizeof(climate_big_number_t),
    .editable = 1,
    .name = "climate_big_number"
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * climate_big_number_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&climate_big_number_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void climate_big_number_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    climate_big_number_t * widget = (climate_big_number_t *)obj;
    static lv_style_t style_base;
    static lv_style_t style_addon;
    static lv_style_t style_addon_top;
    static lv_style_t style_addon_bottom;
    static lv_style_t debug;
    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_base);
        lv_style_init(&style_addon);
        lv_style_init(&style_addon_top);
        lv_style_init(&style_addon_bottom);
        lv_style_init(&debug);

        lv_style_set_bg_opa(&style_base, (255 * 0 / 100));
        lv_style_set_layout(&style_base, LV_LAYOUT_FLEX);
        lv_style_set_flex_flow(&style_base, LV_FLEX_FLOW_ROW);
        lv_style_set_flex_main_place(&style_base, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_cross_place(&style_base, LV_FLEX_ALIGN_END);
        lv_style_set_flex_track_place(&style_base, LV_FLEX_ALIGN_START);
        lv_style_set_width(&style_base, LV_SIZE_CONTENT);
        lv_style_set_height(&style_base, LV_SIZE_CONTENT);
        lv_style_set_width(&style_addon, LV_SIZE_CONTENT);
        lv_style_set_height(&style_addon, LV_SIZE_CONTENT);
        lv_style_set_pad_top(&style_addon, 0);
        lv_style_set_pad_bottom(&style_addon, 0);
        lv_style_set_layout(&style_addon, LV_LAYOUT_FLEX);
        lv_style_set_flex_flow(&style_addon_top, LV_FLEX_FLOW_COLUMN_REVERSE);
        lv_style_set_flex_main_place(&style_addon_top, LV_FLEX_ALIGN_START);
        lv_style_set_flex_cross_place(&style_addon_top, LV_FLEX_ALIGN_START);
        lv_style_set_flex_track_place(&style_addon_top, LV_FLEX_ALIGN_END);
        lv_style_set_flex_flow(&style_addon_bottom, LV_FLEX_FLOW_ROW);
        lv_style_set_flex_main_place(&style_addon_bottom, LV_FLEX_ALIGN_START);
        lv_style_set_flex_cross_place(&style_addon_bottom, LV_FLEX_ALIGN_END);
        lv_style_set_bg_color(&debug, lv_color_hex(0xFF0000));
        lv_style_set_bg_opa(&debug, (255 * 100 / 100));

        style_inited = true;
    }

    lv_obj_t * the_root = NULL;
    lv_obj_set_flag(obj, LV_OBJ_FLAG_SCROLLABLE, false);

    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &style_base, 0);
    lv_obj_t * integer_label = lv_label_create(obj);
    lv_label_set_text(integer_label, "99");
    lv_obj_set_style_pad_all(integer_label, 0, 0);
    widget->integer_label = integer_label;

    lv_obj_t * addon = lv_obj_create(obj);
    lv_obj_set_flag(addon, LV_OBJ_FLAG_CLICKABLE, false);
    lv_obj_set_flag(addon, LV_OBJ_FLAG_SCROLLABLE, false);
    widget->addon = addon;
    lv_obj_remove_style_all(addon);
    lv_obj_add_style(addon, &style_addon, 0);
    lv_obj_add_style(addon, &style_addon_top, 0);
    lv_obj_t * decimal_label = lv_label_create(addon);
    lv_label_set_text(decimal_label, ".99");
    lv_obj_set_style_pad_all(decimal_label, 0, 0);
    widget->decimal_label = decimal_label;

    lv_obj_t * unit_label = lv_label_create(addon);
    lv_label_set_text(unit_label, "%");
    lv_obj_set_style_pad_all(unit_label, 0, 0);
    widget->unit_label = unit_label;

    the_root = obj;
    climate_big_number_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void climate_big_number_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    climate_big_number_destructor_hook(obj);
}

static void climate_big_number_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    /* Call the ancestor's event handler */
    res = lv_obj_event_base(&climate_big_number_class, e);
    if(res != LV_RESULT_OK) return;

    climate_big_number_event_hook(e);
}

