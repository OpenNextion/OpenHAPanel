#ifndef OPEN_NEXTION_MAIN_SCREEN_SYSTEM_STATUS_BAR_H
#define OPEN_NEXTION_MAIN_SCREEN_SYSTEM_STATUS_BAR_H

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_STATUS_BAR_HEIGHT 44

typedef struct system_status_bar system_status_bar_t;

/** Create the shared Home/Wi-Fi/time status bar. The parent owns its object tree. */
system_status_bar_t *system_status_bar_create(lv_obj_t *parent);

/** Return the root LVGL object of the status bar. */
lv_obj_t *system_status_bar_get_obj(system_status_bar_t *bar);

/** Set the optional title displayed on the left side of the status bar. */
void system_status_bar_set_title(system_status_bar_t *bar, const char *title);

/** Update the time displayed on the right side of the status bar. */
void system_status_bar_set_time(system_status_bar_t *bar, const char *time_text);

/** Show or hide the firmware-update indicator on every shared status bar. */
void system_status_bar_set_ota_update_visible(bool visible);

/** Override the global firmware-update indicator state for one status bar. */
void system_status_bar_set_ota_update_visibility_override(system_status_bar_t *bar,
                                                          bool enabled,
                                                          bool visible);

/** Show the Home Assistant indicator only while its data WebSocket is authenticated. */
void system_status_bar_set_hass_connected_visible(bool visible);

/** Show the Wi-Fi indicator only while the STA interface has a usable IP address. */
void system_status_bar_set_wifi_sta_connected_visible(bool visible);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_SYSTEM_STATUS_BAR_H
