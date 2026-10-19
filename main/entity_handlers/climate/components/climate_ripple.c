/**
 * @file climate_ripple.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "climate_ripple_gen.h"
#include "climate_ripple_private_gen.h"
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define DEFAULT_COLOR           0x727272
#define DEFAULT_HOVER_OPACITY   8
#define DEFAULT_PRESSED_OPACITY 12
#define DEFAULT_RADIUS          8

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void control_event_cb(lv_event_t * e);
static void stop_animations(climate_ripple_t * st);
static void start_ripple(climate_ripple_t * st, int32_t x, int32_t y);
static int32_t ripple_end_size(const climate_ripple_t * st);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static lv_opa_t percent_to_opa(uint8_t percent)
{
    if(percent > 100) percent = 100;
    return (lv_opa_t)((percent * LV_OPA_COVER) / 100);
}

static int32_t max_i32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static void ripple_size_anim_cb(void * var, int32_t value)
{
    lv_obj_t * obj = (lv_obj_t *)var;
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL) return;
    lv_obj_set_size(obj, value, value);
    lv_obj_set_pos(obj, st->ripple_x - value / 2, st->ripple_y - value / 2);
}

static void ripple_opa_anim_cb(void * var, int32_t value)
{
    lv_obj_t * obj = (lv_obj_t *)var;
    if(obj == NULL) return;
    lv_obj_set_style_bg_opa(obj, (lv_opa_t)value, 0);
}

static int32_t ripple_end_size(const climate_ripple_t * st)
{
    if(st == NULL || st->control == NULL) return 0;

    int32_t width = lv_obj_get_width(st->control);
    int32_t height = lv_obj_get_height(st->control);
    int32_t left = st->ripple_x;
    int32_t right = width - st->ripple_x;
    int32_t top = st->ripple_y;
    int32_t bottom = height - st->ripple_y;
    return max_i32(max_i32(left, right), max_i32(top, bottom)) * 2;
}

static void stop_animations(climate_ripple_t * st)
{
    if(st == NULL) return;
    lv_obj_t * obj = (lv_obj_t *)st;
    lv_anim_delete(obj, ripple_size_anim_cb);
    lv_anim_delete(obj, ripple_opa_anim_cb);
}

static void start_ripple(climate_ripple_t * st, int32_t x, int32_t y)
{
    if(st == NULL || st->control == NULL || st->disabled) return;
    lv_obj_t * obj = (lv_obj_t *)st;

    int32_t width = lv_obj_get_width(st->control);
    int32_t height = lv_obj_get_height(st->control);
    if(width <= 0 || height <= 0) return;

    stop_animations(st);

    st->ripple_x = x;
    st->ripple_y = y;
    int32_t start_size = max_i32(width, height) / 6;
    if(start_size < 8) start_size = 8;
    int32_t end_size = ripple_end_size(st);
    if(end_size < start_size) end_size = start_size;

    lv_obj_set_style_bg_color(obj, st->color, 0);
    lv_obj_set_style_bg_opa(obj, percent_to_opa(st->pressed_opacity), 0);
    lv_obj_set_style_radius(obj, end_size / 2, 0);
    lv_obj_set_size(obj, start_size, start_size);
    lv_obj_set_pos(obj, x - start_size / 2, y - start_size / 2);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(obj);

    lv_anim_t size_anim;
    lv_anim_init(&size_anim);
    lv_anim_set_var(&size_anim, obj);
    lv_anim_set_values(&size_anim, start_size, end_size);
    lv_anim_set_duration(&size_anim, 220);
    lv_anim_set_path_cb(&size_anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&size_anim, ripple_size_anim_cb);
    lv_anim_start(&size_anim);

    lv_anim_t opa_anim;
    lv_anim_init(&opa_anim);
    lv_anim_set_var(&opa_anim, obj);
    lv_anim_set_values(&opa_anim, percent_to_opa(st->pressed_opacity), 0);
    lv_anim_set_duration(&opa_anim, 320);
    lv_anim_set_path_cb(&opa_anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&opa_anim, ripple_opa_anim_cb);
    lv_anim_start(&opa_anim);
}

static void control_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_user_data(e);
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL || st->disabled) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * control = lv_event_get_target(e);
    if(code == LV_EVENT_PRESSED) {
        lv_point_t point = { lv_obj_get_width(control) / 2, lv_obj_get_height(control) / 2 };
        lv_indev_t * indev = lv_event_get_indev(e);
        if(indev != NULL) {
            lv_point_t screen_point;
            lv_area_t coords;
            lv_indev_get_point(indev, &screen_point);
            lv_obj_get_coords(control, &coords);
            point.x = screen_point.x - coords.x1;
            point.y = screen_point.y - coords.y1;
            if(point.x < 0) point.x = 0;
            if(point.y < 0) point.y = 0;
            if(point.x > lv_obj_get_width(control)) point.x = lv_obj_get_width(control);
            if(point.y > lv_obj_get_height(control)) point.y = lv_obj_get_height(control);
        }
        start_ripple(st, point.x, point.y);
    }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void climate_ripple_constructor_hook(lv_obj_t * obj)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    st->color = lv_color_hex(DEFAULT_COLOR);
    st->hover_opacity = DEFAULT_HOVER_OPACITY;
    st->pressed_opacity = DEFAULT_PRESSED_OPACITY;
    st->radius = DEFAULT_RADIUS;
    st->disabled = false;
    st->control = NULL;

    lv_obj_add_flag(obj, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    if(lv_obj_get_parent(obj) != NULL) {
        climate_ripple_attach(obj, lv_obj_get_parent(obj));
    }
}

void climate_ripple_destructor_hook(lv_obj_t * obj)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    climate_ripple_detach(obj);
    stop_animations(st);
}

void climate_ripple_event_hook(lv_event_t * e)
{
}

void climate_ripple_attach(lv_obj_t * obj, lv_obj_t * control)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL) return;
    if(st->control == control) return;
    climate_ripple_detach(obj);
    st->control = control;
    if(control == NULL) return;
    lv_obj_add_flag(control, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_clip_corner(control, true, 0);
    lv_obj_add_event_cb(control, control_event_cb, LV_EVENT_PRESSED, obj);
}

void climate_ripple_detach(lv_obj_t * obj)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL || st->control == NULL) return;
    lv_obj_remove_event_cb_with_user_data(st->control, control_event_cb, obj);
    st->control = NULL;
}

void climate_ripple_set_p_disabled(lv_obj_t * obj, bool disabled)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL) return;
    st->disabled = disabled;
    if(disabled) {
        stop_animations(st);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void climate_ripple_set_p_color(lv_obj_t * obj, lv_color_t color)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL) return;
    st->color = color;
    lv_obj_set_style_bg_color(obj, color, 0);
}

void climate_ripple_set_p_hover_opacity(lv_obj_t * obj, int32_t opacity_percent)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL) return;
    st->hover_opacity = opacity_percent > 100 ? 100 : opacity_percent;
}

void climate_ripple_set_p_pressed_opacity(lv_obj_t * obj, int32_t opacity_percent)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL) return;
    st->pressed_opacity = opacity_percent > 100 ? 100 : opacity_percent;
}

void climate_ripple_set_p_radius(lv_obj_t * obj, int32_t radius)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    if(st == NULL) return;
    st->radius = radius < 0 ? 0 : radius;
}

bool climate_ripple_get_disabled(lv_obj_t * obj)
{
    climate_ripple_t * st = (climate_ripple_t *)obj;
    return st != NULL && st->disabled;
}
