#ifndef OPEN_NEXTION_LIGHT_ASSUMED_POWER_BUTTONS_H
#define OPEN_NEXTION_LIGHT_ASSUMED_POWER_BUTTONS_H

#include <stdbool.h>

#include "lvgl.h"

typedef void (*light_assumed_power_buttons_change_cb_t)(bool on,
                                                        void *user_data);

lv_obj_t *light_assumed_power_buttons_create(lv_obj_t *parent);
void light_assumed_power_buttons_set_state(lv_obj_t *obj,
                                           bool known,
                                           bool on);
void light_assumed_power_buttons_set_disabled(lv_obj_t *obj, bool disabled);
void light_assumed_power_buttons_set_change_cb(
    lv_obj_t *obj,
    light_assumed_power_buttons_change_cb_t callback,
    void *user_data);

#endif
