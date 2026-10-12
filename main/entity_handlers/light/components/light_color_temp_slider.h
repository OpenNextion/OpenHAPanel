#ifndef OPEN_NEXTION_LIGHT_COLOR_TEMP_SLIDER_H
#define OPEN_NEXTION_LIGHT_COLOR_TEMP_SLIDER_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "light_component_types.h"

typedef void (*light_color_temp_change_cb_t)(uint16_t kelvin,
                                             light_control_phase_t phase,
                                             void *user_data);

lv_obj_t *light_color_temp_slider_create(lv_obj_t *parent);
void light_color_temp_slider_set_range(lv_obj_t *obj,
                                       uint16_t min_kelvin,
                                       uint16_t max_kelvin);
void light_color_temp_slider_set_value(lv_obj_t *obj,
                                       uint16_t kelvin,
                                       lv_anim_enable_t animate);
void light_color_temp_slider_set_marker_visible(lv_obj_t *obj, bool visible);
void light_color_temp_slider_set_disabled(lv_obj_t *obj, bool disabled);
void light_color_temp_slider_set_change_cb(
    lv_obj_t *obj,
    light_color_temp_change_cb_t callback,
    void *user_data);

#endif
