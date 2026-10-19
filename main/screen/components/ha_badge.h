#ifndef OPEN_NEXTION_MAIN_SCREEN_HA_BADGE_H
#define OPEN_NEXTION_MAIN_SCREEN_HA_BADGE_H

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t ha_badge_class;

typedef void (*ha_badge_action_cb_t)(lv_obj_t *badge, void *user_data);

lv_obj_t *ha_badge_create(lv_obj_t *parent);
void ha_badge_set_icon(lv_obj_t *obj, const char *icon);
void ha_badge_set_text(lv_obj_t *obj, const char *text);
void ha_badge_set_icon_color(lv_obj_t *obj, lv_color_t color);
void ha_badge_set_text_color(lv_obj_t *obj, lv_color_t color);
void ha_badge_set_body_clickable(lv_obj_t *obj, bool clickable);
void ha_badge_set_body_click_cb(lv_obj_t *obj, ha_badge_action_cb_t callback,
                                void *user_data);
void ha_badge_set_body_hold_cb(lv_obj_t *obj, ha_badge_action_cb_t callback,
                               void *user_data);
void ha_badge_stop_ripple(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_HA_BADGE_H