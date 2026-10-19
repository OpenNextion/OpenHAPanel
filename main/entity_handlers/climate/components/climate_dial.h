#ifndef OPEN_NEXTION_CLIMATE_DIAL_H
#define OPEN_NEXTION_CLIMATE_DIAL_H

#include "lvgl.h"

typedef void (*climate_dial_change_cb_t)(int value_tenths, bool is_low, void *user_data);
typedef void (*climate_dial_selection_cb_t)(bool is_high, void *user_data);

lv_obj_t *climate_dial_create(lv_obj_t *parent);
void climate_dial_set_range(lv_obj_t *obj, int min_tenths, int max_tenths, int step_tenths);
void climate_dial_set_dual(lv_obj_t *obj, bool dual);
void climate_dial_set_value(lv_obj_t *obj, int value_tenths);
void climate_dial_set_high(lv_obj_t *obj, int value_tenths);
void climate_dial_set_measured(lv_obj_t *obj, int value_tenths);
int climate_dial_get_value(lv_obj_t *obj);
bool climate_dial_selected_low(lv_obj_t *obj);
bool climate_dial_is_dragging(lv_obj_t *obj);
void climate_dial_set_selected_high(lv_obj_t *obj, bool selected_high);
void climate_dial_set_currents(lv_obj_t *obj, const char *temperature,
                               const char *humidity);
void climate_dial_set_action(lv_obj_t *obj, const char *text);
void climate_dial_set_unit(lv_obj_t *obj, const char *unit);
void climate_dial_set_theme(lv_obj_t *obj, const char *mode, bool humidity);
void climate_dial_set_disabled(lv_obj_t *obj, bool disabled);
void climate_dial_set_change_cb(lv_obj_t *obj, climate_dial_change_cb_t callback, void *user_data);
void climate_dial_set_selection_cb(lv_obj_t *obj, climate_dial_selection_cb_t callback, void *user_data);

#endif
