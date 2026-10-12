#ifndef OPEN_NEXTION_MAIN_SCREEN_HA_TILE_CARD_H
#define OPEN_NEXTION_MAIN_SCREEN_HA_TILE_CARD_H

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

LV_ATTRIBUTE_EXTERN_DATA extern const lv_obj_class_t ha_tile_card_class;

typedef void (*ha_tile_card_icon_click_cb_t)(lv_obj_t *tile_card, void *user_data);
typedef void (*ha_tile_card_body_click_cb_t)(lv_obj_t *tile_card, void *user_data);
typedef void (*ha_tile_card_body_hold_cb_t)(lv_obj_t *tile_card, void *user_data);

/** Create an HA tile card owned by parent. Delete it with lv_obj_delete(). */
lv_obj_t *ha_tile_card_create(lv_obj_t *parent);

void ha_tile_card_set_title(lv_obj_t *obj, const char *title);
/** Set static state text and remove any previous relative-time binding. */
void ha_tile_card_set_state_text(lv_obj_t *obj, const char *state_text);
/** Keep the state label synchronized with an event epoch. */
esp_err_t ha_tile_card_set_relative_state_time(lv_obj_t *obj,
                                               time_t event_epoch);
void ha_tile_card_set_icon(lv_obj_t *obj, const char *icon);
/** Show or hide the unavailable warning badge over the icon circle. */
void ha_tile_card_set_unavailable(lv_obj_t *obj, bool unavailable);
/** Enable or disable pointer events on the icon circle. Disabled by default. */
void ha_tile_card_set_icon_clickable(lv_obj_t *obj, bool clickable);
/** Set the callback invoked by an enabled icon after LV_EVENT_CLICKED. */
void ha_tile_card_set_icon_click_cb(lv_obj_t *obj,
                                    ha_tile_card_icon_click_cb_t callback,
                                    void *user_data);
/** Enable or disable interaction with the card body. Enabled by default. */
void ha_tile_card_set_body_clickable(lv_obj_t *obj, bool clickable);
/** Set the callback for clicks outside the independently clickable icon. */
void ha_tile_card_set_body_click_cb(lv_obj_t *obj,
                                    ha_tile_card_body_click_cb_t callback,
                                    void *user_data);
/** Set the callback invoked once when the card body is held. */
void ha_tile_card_set_body_hold_cb(lv_obj_t *obj,
                                   ha_tile_card_body_hold_cb_t callback,
                                   void *user_data);
/** Set the accent color used by the card-body press ripple. */
void ha_tile_card_set_ripple_color(lv_obj_t *obj, lv_color_t color);
/** Immediately stop and clear the card-body press ripple. */
void ha_tile_card_stop_ripple(lv_obj_t *obj);
void ha_tile_card_set_background_color(lv_obj_t *obj, lv_color_t color);
void ha_tile_card_set_icon_color(lv_obj_t *obj, lv_color_t color);
void ha_tile_card_set_icon_circle_background_color(lv_obj_t *obj, lv_color_t color);
void ha_tile_card_set_title_color(lv_obj_t *obj, lv_color_t color);
void ha_tile_card_set_state_color(lv_obj_t *obj, lv_color_t color);
/** Set the optional icon displayed immediately before the state text. */
void ha_tile_card_set_state_icon(lv_obj_t *obj, const char *icon);
/** Show or hide the optional state icon without changing its configured text. */
void ha_tile_card_set_state_icon_visible(lv_obj_t *obj, bool visible);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_HA_TILE_CARD_H
