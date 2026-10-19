#ifndef OPEN_NEXTION_CLIMATE_TARGET_TOGGLE_H
#define OPEN_NEXTION_CLIMATE_TARGET_TOGGLE_H

#include "lvgl.h"

typedef void (*climate_target_toggle_cb_t)(bool humidity, void *user_data);

lv_obj_t *climate_target_toggle_create(lv_obj_t *parent);
void climate_target_toggle_set_selected(lv_obj_t *obj, bool humidity);
void climate_target_toggle_set_humidity_available(lv_obj_t *obj, bool available);
void climate_target_toggle_set_callback(lv_obj_t *obj, climate_target_toggle_cb_t callback, void *user_data);

#endif
