#ifndef OPEN_NEXTION_LIGHT_BRIGHTNESS_SLIDER_H
#define OPEN_NEXTION_LIGHT_BRIGHTNESS_SLIDER_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "light_component_types.h"

typedef void (*light_brightness_change_cb_t)(uint8_t percent,
                                             light_control_phase_t phase,
                                             void *user_data);

lv_obj_t *light_brightness_slider_create(lv_obj_t *parent);
void light_brightness_slider_set_value(lv_obj_t *obj,
                                       uint8_t percent,
                                       lv_anim_enable_t animate);
void light_brightness_slider_set_active_color(lv_obj_t *obj, lv_color_t color);
void light_brightness_slider_set_power(lv_obj_t *obj, bool on);
void light_brightness_slider_set_disabled(lv_obj_t *obj, bool disabled);
void light_brightness_slider_set_change_cb(
    lv_obj_t *obj,
    light_brightness_change_cb_t callback,
    void *user_data);

#endif
