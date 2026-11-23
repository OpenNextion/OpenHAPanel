#ifndef OPEN_NEXTION_CLIMATE_TARGET_TEMPERATURE_H
#define OPEN_NEXTION_CLIMATE_TARGET_TEMPERATURE_H

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"

typedef struct {
    double value;
    double min;
    double max;
    double step;
} climate_temperature_target_t;

typedef void (*climate_temperature_change_cb_t)(size_t index, int direction,
                                                 void *user_data);

lv_obj_t *climate_target_temperature_create(lv_obj_t *parent);
void climate_target_temperature_set(lv_obj_t *obj,
                                    const climate_temperature_target_t *targets,
                                    size_t count, const char *unit,
                                    bool available, bool half_width);
size_t climate_target_temperature_get(lv_obj_t *obj,
                                      climate_temperature_target_t targets[2]);
void climate_target_temperature_set_change_cb(lv_obj_t *obj,
                                              climate_temperature_change_cb_t callback,
                                              void *user_data);
void climate_target_temperature_apply_theme(lv_obj_t *obj);

#endif
