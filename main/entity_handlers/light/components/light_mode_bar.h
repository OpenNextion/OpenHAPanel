#ifndef OPEN_NEXTION_LIGHT_MODE_BAR_H
#define OPEN_NEXTION_LIGHT_MODE_BAR_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "light_component_types.h"

typedef enum {
    LIGHT_MODE_BAR_EVENT_POWER = 0,
    LIGHT_MODE_BAR_EVENT_MODE,
} light_mode_bar_event_type_t;

typedef struct {
    light_mode_bar_event_type_t type;
    light_mode_t mode;
} light_mode_bar_event_t;

typedef void (*light_mode_bar_event_cb_t)(
    const light_mode_bar_event_t *event,
    void *user_data);

lv_obj_t *light_mode_bar_create(lv_obj_t *parent);
void light_mode_bar_set_modes(lv_obj_t *obj, uint32_t mode_mask);
void light_mode_bar_set_active_mode(lv_obj_t *obj, light_mode_t mode);
void light_mode_bar_set_power(lv_obj_t *obj, bool on);
void light_mode_bar_set_disabled(lv_obj_t *obj, bool disabled);
void light_mode_bar_set_event_cb(lv_obj_t *obj,
                                 light_mode_bar_event_cb_t callback,
                                 void *user_data);

#endif
