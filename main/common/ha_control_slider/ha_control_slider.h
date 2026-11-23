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
/** Set the outer track radius in pixels (clamped to 0-28). Default: 12.
 * The inner fill end keeps its 8px radius. */
void ha_control_slider_set_radius(lv_obj_t *obj, int32_t radius);
void ha_control_slider_set_value(lv_obj_t *obj, uint8_t percent,
                                 lv_anim_enable_t animate);
void ha_control_slider_set_active_color(lv_obj_t *obj, lv_color_t color);
/** Set the fill color exactly as given, skipping the saturation-based
 * contrast correction. Used for disabled/neutral grays that must not be
 * brightened. */
void ha_control_slider_set_active_color_raw(lv_obj_t *obj, lv_color_t color);
/** Override the track color and opacity displayed when the value is zero. */
void ha_control_slider_set_off_track_style(lv_obj_t *obj, lv_color_t color,
                                           lv_opa_t opacity);
/** Override the active track background. The zero-value track keeps its theme
 * default unless overridden with ha_control_slider_set_off_track_style(). */
void ha_control_slider_set_background_color(lv_obj_t *obj, lv_color_t color);
/** Restore the automatic active-track tint over the parent surface. */
void ha_control_slider_use_auto_background(lv_obj_t *obj);
/** Enable a strictly-linear fill that grows from the track's left inner edge
 * by percentage (a low value shrinks it to a tiny stub). Disabled by default,
 * where the fill keeps the fixed 24px end-cap used by brightness/cover/light.
 * @see BRIGHTNESS_END_CAP in ha_control_slider.c */
void ha_control_slider_set_linear_fill(lv_obj_t *obj, bool linear);
void ha_control_slider_set_power(lv_obj_t *obj, bool on);
void ha_control_slider_set_disabled(lv_obj_t *obj, bool disabled);
void ha_control_slider_set_change_cb(lv_obj_t *obj,
                                      ha_control_slider_change_cb_t callback,
                                      void *user_data);
/** Enable color-temperature mode: the track renders a horizontal
 * warm-to-cool gradient across [min_kelvin, max_kelvin] while the value
 * stays a 0-100 percent position. The caller converts percent to kelvin. */
void ha_control_slider_set_color_temp_range(lv_obj_t *obj,
                                            uint16_t min_kelvin,
                                            uint16_t max_kelvin);
/** Re-render the control after the active screen theme changes. The
 * color-temperature cursor's indicator line follows the light/dark theme. */
void ha_control_slider_refresh_theme(lv_obj_t *obj);

#endif
