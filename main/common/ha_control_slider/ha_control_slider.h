#ifndef OPEN_NEXTION_HA_CONTROL_SLIDER_H
#define OPEN_NEXTION_HA_CONTROL_SLIDER_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

typedef enum {
    HA_CONTROL_SLIDER_PREVIEW,
    HA_CONTROL_SLIDER_COMMIT,
    HA_CONTROL_SLIDER_CANCEL,
} ha_control_slider_phase_t;

typedef void (*ha_control_slider_change_cb_t)(uint8_t percent,
                                               ha_control_slider_phase_t phase,
                                               void *user_data);

/** A 56px high, parent-width horizontal slider with no HA dependencies. */
lv_obj_t *ha_control_slider_create(lv_obj_t *parent);
/** Set the outer track radius in pixels (clamped to 0–28). Default: 12.
 * The inner fill end keeps its 8px radius. */
void ha_control_slider_set_radius(lv_obj_t *obj, int32_t radius);
void ha_control_slider_set_value(lv_obj_t *obj, uint8_t percent,
                                 lv_anim_enable_t animate);
void ha_control_slider_set_active_color(lv_obj_t *obj, lv_color_t color);
/** Override the track background. Without an override the active track is
 * drawn with the original slider's 20% color tint over its parent surface. */
void ha_control_slider_set_background_color(lv_obj_t *obj, lv_color_t color);
/** Restore the automatic active/off tints over the parent surface. */
void ha_control_slider_use_auto_background(lv_obj_t *obj);
void ha_control_slider_set_power(lv_obj_t *obj, bool on);
void ha_control_slider_set_disabled(lv_obj_t *obj, bool disabled);
void ha_control_slider_set_change_cb(lv_obj_t *obj,
                                      ha_control_slider_change_cb_t callback,
                                      void *user_data);

#endif
