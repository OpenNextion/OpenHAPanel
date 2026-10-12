#ifndef OPEN_NEXTION_LIGHT_POWER_TOGGLE_H
#define OPEN_NEXTION_LIGHT_POWER_TOGGLE_H

#include <stdbool.h>

#include "lvgl.h"

typedef void (*light_power_toggle_change_cb_t)(bool on, void *user_data);

lv_obj_t *light_power_toggle_create(lv_obj_t *parent);
void light_power_toggle_set_value(lv_obj_t *obj,
                                  bool on,
                                  lv_anim_enable_t animate);
void light_power_toggle_set_disabled(lv_obj_t *obj, bool disabled);
void light_power_toggle_set_change_cb(
    lv_obj_t *obj,
    light_power_toggle_change_cb_t callback,
    void *user_data);

#endif
