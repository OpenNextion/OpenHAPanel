#ifndef OPEN_NEXTION_LIGHT_RGB_PICKER_H
#define OPEN_NEXTION_LIGHT_RGB_PICKER_H

#include <stdbool.h>

#include "lvgl.h"
#include "light_component_types.h"

typedef void (*light_rgb_change_cb_t)(light_rgb_t rgb,
                                      light_control_phase_t phase,
                                      void *user_data);

lv_obj_t *light_rgb_picker_create(lv_obj_t *parent);
void light_rgb_picker_set_value(lv_obj_t *obj, light_rgb_t rgb);
void light_rgb_picker_set_marker_visible(lv_obj_t *obj, bool visible);
void light_rgb_picker_set_disabled(lv_obj_t *obj, bool disabled);
void light_rgb_picker_set_change_cb(lv_obj_t *obj,
                                    light_rgb_change_cb_t callback,
                                    void *user_data);

#endif
