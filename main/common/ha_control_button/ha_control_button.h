// main/common/ha_control_button/ha_control_button.h
#ifndef OPEN_NEXTION_HA_CONTROL_BUTTON_H
#define OPEN_NEXTION_HA_CONTROL_BUTTON_H

#include <stdbool.h>
#include "lvgl.h"

typedef void (*ha_control_button_press_cb_t)(void *user_data);

lv_obj_t *ha_control_button_create(lv_obj_t *parent);
void ha_control_button_set_disabled(lv_obj_t *obj, bool disabled);
void ha_control_button_set_press_cb(lv_obj_t *obj,
                                    ha_control_button_press_cb_t callback,
                                    void *user_data);
void ha_control_button_refresh_theme(lv_obj_t *obj);

/* Optional action-result animation (matches the Home Assistant "Press"
 * button behaviour): enter a busy state that shows a spinner after a short
 * delay, then display a success/error icon that resets after a moment. */
void ha_control_button_begin_loading(lv_obj_t *obj);
void ha_control_button_finish_loading(lv_obj_t *obj, bool success);
bool ha_control_button_is_busy(lv_obj_t *obj);

#endif
