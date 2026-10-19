/**
 * @file climate_control_circular_slider_gen.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_control_circular_slider_private_gen.h"
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

void climate_control_circular_slider_constructor_hook(lv_obj_t * obj);
void climate_control_circular_slider_destructor_hook(lv_obj_t * obj);
void climate_control_circular_slider_event_hook(lv_event_t * e);

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void climate_control_circular_slider_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_control_circular_slider_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void climate_control_circular_slider_event(const lv_obj_class_t * class_p, lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t climate_control_circular_slider_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = climate_control_circular_slider_constructor,
    .destructor_cb = climate_control_circular_slider_destructor,
    .event_cb = climate_control_circular_slider_event,
    .instance_size = sizeof(climate_control_circular_slider_t),
    .editable = 1,
    .name = "climate_control_circular_slider"
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * climate_control_circular_slider_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&climate_control_circular_slider_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void climate_control_circular_slider_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    climate_control_circular_slider_t * widget = (climate_control_circular_slider_t *)obj;
    static lv_style_t style_base;
    static lv_style_t low_arc_main;
    static lv_style_t low_arc_indicator;
    static lv_style_t low_arc_knob;
    static lv_style_t low_active_arc_main;
    static lv_style_t low_active_arc_indicator;
    static lv_style_t low_active_arc_knob;
    static lv_style_t high_arc_main;
    static lv_style_t high_arc_indicator;
    static lv_style_t high_arc_knob;
    static lv_style_t high_active_arc_main;
    static lv_style_t high_active_arc_indicator;
    static lv_style_t high_active_arc_knob;
    static lv_style_t bg_arc_knob;
    static lv_style_t bg_arc_main;
    static lv_style_t bg_arc_indicator;
    static lv_style_t dot_arc_main;
    static lv_style_t dot_arc_indicator;
    static lv_style_t cur_dot_arc_knob;
    static lv_style_t low_dot_arc_knob;
    static lv_style_t high_dot_arc_knob;
    static lv_style_t touch_arc_knob;
    static lv_style_t touch_arc_main;
    static lv_style_t touch_arc_indicator;
    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_base);
        lv_style_init(&low_arc_main);
        lv_style_init(&low_arc_indicator);
        lv_style_init(&low_arc_knob);
        lv_style_init(&low_active_arc_main);
        lv_style_init(&low_active_arc_indicator);
        lv_style_init(&low_active_arc_knob);
        lv_style_init(&high_arc_main);
        lv_style_init(&high_arc_indicator);
        lv_style_init(&high_arc_knob);
        lv_style_init(&high_active_arc_main);
        lv_style_init(&high_active_arc_indicator);
        lv_style_init(&high_active_arc_knob);
        lv_style_init(&bg_arc_knob);
        lv_style_init(&bg_arc_main);
        lv_style_init(&bg_arc_indicator);
        lv_style_init(&dot_arc_main);
        lv_style_init(&dot_arc_indicator);
        lv_style_init(&cur_dot_arc_knob);
        lv_style_init(&low_dot_arc_knob);
        lv_style_init(&high_dot_arc_knob);
        lv_style_init(&touch_arc_knob);
        lv_style_init(&touch_arc_main);
        lv_style_init(&touch_arc_indicator);

        lv_style_set_bg_opa(&style_base, (255 * 0 / 100));
        lv_style_set_arc_opa(&low_arc_main, (255 * 0 / 100));
        lv_style_set_arc_color(&low_arc_indicator, lv_color_hex(0xffb790));
        lv_style_set_arc_opa(&low_arc_indicator, (255 * 100 / 100));
        lv_style_set_opa(&low_arc_knob, (255 * 0 / 100));
        lv_style_set_arc_opa(&low_active_arc_main, (255 * 0 / 100));
        lv_style_set_arc_color(&low_active_arc_indicator, lv_color_hex(0xff6f22));
        lv_style_set_arc_opa(&low_active_arc_indicator, (255 * 100 / 100));
        lv_style_set_opa(&low_active_arc_knob, (255 * 0 / 100));
        lv_style_set_arc_opa(&high_arc_main, (255 * 0 / 100));
        lv_style_set_arc_color(&high_arc_indicator, lv_color_hex(0x90caf9));
        lv_style_set_arc_opa(&high_arc_indicator, (255 * 100 / 100));
        lv_style_set_opa(&high_arc_knob, (255 * 0 / 100));
        lv_style_set_arc_opa(&high_active_arc_main, (255 * 0 / 100));
        lv_style_set_arc_color(&high_active_arc_indicator, lv_color_hex(0x2196f3));
        lv_style_set_arc_opa(&high_active_arc_indicator, (255 * 100 / 100));
        lv_style_set_opa(&high_active_arc_knob, (255 * 0 / 100));
        lv_style_set_opa(&bg_arc_knob, (255 * 0 / 100));
        lv_style_set_arc_color(&bg_arc_main, lv_color_hex(0xe0e0e0));
        lv_style_set_arc_opa(&bg_arc_main, (255 * 30 / 100));
        lv_style_set_arc_opa(&bg_arc_indicator, (255 * 0 / 100));
        lv_style_set_arc_opa(&dot_arc_main, (255 * 0 / 100));
        lv_style_set_arc_opa(&dot_arc_indicator, (255 * 0 / 100));
        lv_style_set_opa(&cur_dot_arc_knob, LV_OPA_COVER);
        lv_style_set_bg_color(&cur_dot_arc_knob, lv_color_hex(0x959595));
        lv_style_set_pad_all(&cur_dot_arc_knob, -2);
        lv_style_set_opa(&low_dot_arc_knob, (255 * 100 / 100));
        lv_style_set_bg_color(&low_dot_arc_knob, lv_color_hex(0xFFFFFF));
        lv_style_set_pad_all(&low_dot_arc_knob, -2);
        lv_style_set_opa(&high_dot_arc_knob, (255 * 100 / 100));
        lv_style_set_bg_color(&high_dot_arc_knob, lv_color_hex(0xFFFFFF));
        lv_style_set_pad_all(&high_dot_arc_knob, -2);
        lv_style_set_opa(&touch_arc_knob, (255 * 0 / 100));
        lv_style_set_arc_opa(&touch_arc_main, (255 * 0 / 100));
        lv_style_set_arc_opa(&touch_arc_indicator, (255 * 0 / 100));

        style_inited = true;
    }

    lv_obj_t * the_root = NULL;
    lv_obj_add_style(obj, &style_base, 0);
    lv_obj_t * bg_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(bg_arc, 0);
    lv_arc_set_bg_end_angle(bg_arc, 270);
    lv_arc_set_start_angle(bg_arc, 0);
    lv_arc_set_end_angle(bg_arc, 270);
    lv_arc_set_value(bg_arc, 0);
    lv_arc_set_min_value(bg_arc, 0);
    lv_arc_set_max_value(bg_arc, 100);
    lv_arc_set_rotation(bg_arc, 135);
    lv_obj_set_flag(bg_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->bg_arc = bg_arc;
    lv_obj_add_style(bg_arc, &bg_arc_main, LV_PART_MAIN);
    lv_obj_add_style(bg_arc, &bg_arc_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(bg_arc, &bg_arc_knob, LV_PART_KNOB);

    lv_obj_t * low_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(low_arc, 0);
    lv_arc_set_bg_end_angle(low_arc, 70);
    lv_arc_set_start_angle(low_arc, 0);
    lv_arc_set_end_angle(low_arc, 70);
    lv_arc_set_value(low_arc, 1);
    lv_arc_set_min_value(low_arc, 0);
    lv_arc_set_max_value(low_arc, 1);
    lv_arc_set_rotation(low_arc, 135);
    lv_obj_set_flag(low_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->low_arc = low_arc;
    lv_obj_add_style(low_arc, &low_arc_main, LV_PART_MAIN);
    lv_obj_add_style(low_arc, &low_arc_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(low_arc, &low_arc_knob, LV_PART_KNOB);

    lv_obj_t * low_active_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(low_active_arc, 70);
    lv_arc_set_bg_end_angle(low_active_arc, 170);
    lv_arc_set_start_angle(low_active_arc, 70);
    lv_arc_set_end_angle(low_active_arc, 170);
    lv_arc_set_value(low_active_arc, 1);
    lv_arc_set_min_value(low_active_arc, 0);
    lv_arc_set_max_value(low_active_arc, 1);
    lv_arc_set_rotation(low_active_arc, 135);
    lv_obj_set_flag(low_active_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->low_active_arc = low_active_arc;
    lv_obj_add_style(low_active_arc, &low_active_arc_main, LV_PART_MAIN);
    lv_obj_add_style(low_active_arc, &low_active_arc_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(low_active_arc, &low_active_arc_knob, LV_PART_KNOB);

    lv_obj_t * high_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(high_arc, 170);
    lv_arc_set_bg_end_angle(high_arc, 230);
    lv_arc_set_start_angle(high_arc, 170);
    lv_arc_set_end_angle(high_arc, 230);
    lv_arc_set_value(high_arc, 1);
    lv_arc_set_min_value(high_arc, 0);
    lv_arc_set_max_value(high_arc, 1);
    lv_arc_set_rotation(high_arc, 135);
    lv_obj_set_flag(high_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->high_arc = high_arc;
    lv_obj_add_style(high_arc, &high_arc_main, LV_PART_MAIN);
    lv_obj_add_style(high_arc, &high_arc_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(high_arc, &high_arc_knob, LV_PART_KNOB);

    lv_obj_t * high_active_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(high_active_arc, 230);
    lv_arc_set_bg_end_angle(high_active_arc, 270);
    lv_arc_set_start_angle(high_active_arc, 230);
    lv_arc_set_end_angle(high_active_arc, 270);
    lv_arc_set_value(high_active_arc, 1);
    lv_arc_set_min_value(high_active_arc, 0);
    lv_arc_set_max_value(high_active_arc, 1);
    lv_arc_set_rotation(high_active_arc, 135);
    lv_obj_set_flag(high_active_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->high_active_arc = high_active_arc;
    lv_obj_add_style(high_active_arc, &high_active_arc_main, LV_PART_MAIN);
    lv_obj_add_style(high_active_arc, &high_active_arc_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(high_active_arc, &high_active_arc_knob, LV_PART_KNOB);

    lv_obj_t * cur_dot_knob_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(cur_dot_knob_arc, 0);
    lv_arc_set_bg_end_angle(cur_dot_knob_arc, 270);
    lv_arc_set_start_angle(cur_dot_knob_arc, 0);
    lv_arc_set_end_angle(cur_dot_knob_arc, 270);
    lv_arc_set_value(cur_dot_knob_arc, 40);
    lv_arc_set_min_value(cur_dot_knob_arc, 0);
    lv_arc_set_max_value(cur_dot_knob_arc, 100);
    lv_arc_set_rotation(cur_dot_knob_arc, 135);
    lv_obj_set_flag(cur_dot_knob_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->cur_dot_knob_arc = cur_dot_knob_arc;
    lv_obj_add_style(cur_dot_knob_arc, &dot_arc_main, LV_PART_MAIN);
    lv_obj_add_style(cur_dot_knob_arc, &dot_arc_main, LV_PART_INDICATOR);
    lv_obj_add_style(cur_dot_knob_arc, &cur_dot_arc_knob, LV_PART_KNOB);

    lv_obj_t * low_dot_knob_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(low_dot_knob_arc, 0);
    lv_arc_set_bg_end_angle(low_dot_knob_arc, 270);
    lv_arc_set_start_angle(low_dot_knob_arc, 0);
    lv_arc_set_end_angle(low_dot_knob_arc, 270);
    lv_arc_set_value(low_dot_knob_arc, 50);
    lv_arc_set_min_value(low_dot_knob_arc, 0);
    lv_arc_set_max_value(low_dot_knob_arc, 100);
    lv_arc_set_rotation(low_dot_knob_arc, 135);
    lv_obj_set_flag(low_dot_knob_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->low_dot_knob_arc = low_dot_knob_arc;
    lv_obj_add_style(low_dot_knob_arc, &dot_arc_main, LV_PART_MAIN);
    lv_obj_add_style(low_dot_knob_arc, &dot_arc_main, LV_PART_INDICATOR);
    lv_obj_add_style(low_dot_knob_arc, &low_dot_arc_knob, LV_PART_KNOB);

    lv_obj_t * high_dot_knob_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(high_dot_knob_arc, 0);
    lv_arc_set_bg_end_angle(high_dot_knob_arc, 270);
    lv_arc_set_start_angle(high_dot_knob_arc, 0);
    lv_arc_set_end_angle(high_dot_knob_arc, 270);
    lv_arc_set_value(high_dot_knob_arc, 80);
    lv_arc_set_min_value(high_dot_knob_arc, 0);
    lv_arc_set_max_value(high_dot_knob_arc, 100);
    lv_arc_set_rotation(high_dot_knob_arc, 135);
    lv_obj_set_flag(high_dot_knob_arc, LV_OBJ_FLAG_CLICKABLE, false);
    widget->high_dot_knob_arc = high_dot_knob_arc;
    lv_obj_add_style(high_dot_knob_arc, &dot_arc_main, LV_PART_MAIN);
    lv_obj_add_style(high_dot_knob_arc, &dot_arc_main, LV_PART_INDICATOR);
    lv_obj_add_style(high_dot_knob_arc, &low_dot_arc_knob, LV_PART_KNOB);

    lv_obj_t * touch_arc = lv_arc_create(obj);
    lv_arc_set_bg_start_angle(touch_arc, 0);
    lv_arc_set_bg_end_angle(touch_arc, 270);
    lv_arc_set_start_angle(touch_arc, 0);
    lv_arc_set_end_angle(touch_arc, 270);
    lv_arc_set_value(touch_arc, 30);
    lv_arc_set_min_value(touch_arc, 0);
    lv_arc_set_max_value(touch_arc, 100);
    lv_arc_set_rotation(touch_arc, 135);
    widget->touch_arc = touch_arc;
    lv_obj_add_style(touch_arc, &touch_arc_main, LV_PART_MAIN);
    lv_obj_add_style(touch_arc, &touch_arc_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(touch_arc, &touch_arc_knob, LV_PART_KNOB);

    the_root = obj;
    climate_control_circular_slider_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void climate_control_circular_slider_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    climate_control_circular_slider_destructor_hook(obj);
}

static void climate_control_circular_slider_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    /* Call the ancestor's event handler */
    res = lv_obj_event_base(&climate_control_circular_slider_class, e);
    if(res != LV_RESULT_OK) return;

    climate_control_circular_slider_event_hook(e);
}

