#ifndef OPEN_NEXTION_CLIMATE_STEP_BUTTONS_H
#define OPEN_NEXTION_CLIMATE_STEP_BUTTONS_H

#include "lvgl.h"

typedef void (*climate_step_cb_t)(int direction, void *user_data);

lv_obj_t *climate_step_buttons_create(lv_obj_t *parent);
void climate_step_buttons_set_callback(lv_obj_t *obj, climate_step_cb_t callback, void *user_data);
void climate_step_buttons_set_disabled(lv_obj_t *obj, bool disabled);

#endif
