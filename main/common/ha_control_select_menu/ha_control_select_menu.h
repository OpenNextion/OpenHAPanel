#ifndef OPEN_NEXTION_HA_CONTROL_SELECT_MENU_H
#define OPEN_NEXTION_HA_CONTROL_SELECT_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

#define HA_CONTROL_SELECT_MENU_MAX_OPTIONS 16
#define HA_CONTROL_SELECT_MENU_MAX_VALUE_LEN 48

typedef struct {
    const char *value;
    const char *label;
    const char *symbol;
} ha_control_select_menu_option_t;

typedef void (*ha_control_select_menu_change_cb_t)(const char *value, void *user_data);

/* RGB colors use 0xRRGGBB; button_opa applies while the menu is closed. */
typedef struct {
    uint32_t button_color;
    lv_opa_t button_opa;
    uint32_t pressed_color;
    uint32_t text_color;
    uint32_t menu_color;
    uint32_t menu_border_color;
    uint32_t scrollbar_color;
    uint32_t row_pressed_color;
} ha_control_select_menu_colors_t;

lv_obj_t *ha_control_select_menu_create(lv_obj_t *parent);
bool ha_control_select_menu_set_options(lv_obj_t *obj,
                                        const ha_control_select_menu_option_t *options,
                                        size_t count);
void ha_control_select_menu_set_value(lv_obj_t *obj, const char *value);
size_t ha_control_select_menu_get_option_count(lv_obj_t *obj);
void ha_control_select_menu_set_disabled(lv_obj_t *obj, bool disabled);
void ha_control_select_menu_set_change_cb(lv_obj_t *obj,
                                           ha_control_select_menu_change_cb_t callback,
                                           void *user_data);
void ha_control_select_menu_set_title(lv_obj_t *obj, const char *title);
void ha_control_select_menu_set_icon(lv_obj_t *obj, const char *icon);
/* Read the defaults, change selected fields, then set the palette for one theme. */
bool ha_control_select_menu_get_colors(lv_obj_t *obj, bool light,
                                       ha_control_select_menu_colors_t *colors);
void ha_control_select_menu_set_colors(lv_obj_t *obj, bool light,
                                       const ha_control_select_menu_colors_t *colors);
void ha_control_select_menu_apply_theme(lv_obj_t *obj);

#endif
