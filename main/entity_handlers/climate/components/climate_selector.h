#ifndef OPEN_NEXTION_CLIMATE_SELECTOR_H
#define OPEN_NEXTION_CLIMATE_SELECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

typedef void (*climate_selector_change_cb_t)(size_t index, const char *value, void *user_data);

lv_obj_t *climate_selector_create(lv_obj_t *parent);
bool climate_selector_set_options(lv_obj_t *obj, const char *const *options,
                                  const char *const *icons, size_t count);
void climate_selector_set_selected(lv_obj_t *obj, size_t index);
void climate_selector_set_disabled(lv_obj_t *obj, bool disabled);
void climate_selector_set_change_cb(lv_obj_t *obj, climate_selector_change_cb_t callback, void *user_data);
void climate_selector_set_title(lv_obj_t *obj, const char *title);
void climate_selector_set_icon(lv_obj_t *obj, const char *icon);
void climate_selector_set_accent(lv_obj_t *obj, lv_color_t color);
void climate_selector_set_content_layout(lv_obj_t *obj, bool wide);

#endif
