#ifndef OPEN_NEXTION_LIGHT_EFFECT_SELECTOR_H
#define OPEN_NEXTION_LIGHT_EFFECT_SELECTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"

typedef void (*light_effect_change_cb_t)(size_t index,
                                         const char *effect,
                                         void *user_data);

lv_obj_t *light_effect_selector_create(lv_obj_t *parent);
bool light_effect_selector_set_options(lv_obj_t *obj,
                                       const char *const *options,
                                       size_t count);
void light_effect_selector_set_selected(lv_obj_t *obj, size_t index);
void light_effect_selector_set_power(lv_obj_t *obj, bool on);
void light_effect_selector_set_disabled(lv_obj_t *obj, bool disabled);
void light_effect_selector_set_change_cb(lv_obj_t *obj,
                                         light_effect_change_cb_t callback,
                                         void *user_data);

#endif
