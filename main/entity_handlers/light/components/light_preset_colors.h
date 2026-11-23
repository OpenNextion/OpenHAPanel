#ifndef OPEN_NEXTION_LIGHT_PRESET_COLORS_H
#define OPEN_NEXTION_LIGHT_PRESET_COLORS_H

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"
#include "../light_favorite_color.h"

typedef void (*light_preset_select_cb_t)(size_t index,
                                         const light_favorite_color_t *color,
                                         void *user_data);

lv_obj_t *light_preset_colors_create(lv_obj_t *parent);
void light_preset_colors_set_palette(lv_obj_t *obj,
                                     const light_favorite_color_t *colors,
                                     size_t count);
void light_preset_colors_set_disabled(lv_obj_t *obj, bool disabled);
/** Switch to a single tile row, showing at most limit equally sized colors. */
void light_preset_colors_set_tile_layout(lv_obj_t *obj, size_t limit);
void light_preset_colors_refresh_theme(lv_obj_t *obj);
void light_preset_colors_set_select_cb(lv_obj_t *obj,
                                       light_preset_select_cb_t callback,
                                       void *user_data);

#endif
