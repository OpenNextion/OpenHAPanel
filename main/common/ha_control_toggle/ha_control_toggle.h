// main/common/ha_control_toggle/ha_control_toggle.h
#ifndef OPEN_NEXTION_HA_CONTROL_TOGGLE_H
#define OPEN_NEXTION_HA_CONTROL_TOGGLE_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

typedef void (*ha_control_toggle_change_cb_t)(bool on, void *user_data);

lv_obj_t *ha_control_toggle_create(lv_obj_t *parent);
void ha_control_toggle_set_value(lv_obj_t *obj, bool on, lv_anim_enable_t animate);
void ha_control_toggle_set_active_color(lv_obj_t *obj, lv_color_t color);
void ha_control_toggle_set_disabled(lv_obj_t *obj, bool disabled);
void ha_control_toggle_set_change_cb(lv_obj_t *obj,
                                     ha_control_toggle_change_cb_t callback,
                                     void *user_data);
lv_obj_t *ha_control_toggle_get_knob(lv_obj_t *obj);
void ha_control_toggle_set_icon_text(lv_obj_t *obj, const char *text);

#endif
