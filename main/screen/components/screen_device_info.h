#ifndef OPEN_NEXTION_MAIN_SCREEN_SCREEN_DEVICE_INFO_H
#define OPEN_NEXTION_MAIN_SCREEN_SCREEN_DEVICE_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "lvgl.h"

typedef struct screen_device_info screen_device_info_t;
typedef void (*screen_device_info_check_update_cb_t)(screen_device_info_t *view,
                                                     void *user_data);
typedef void (*screen_device_info_upgrade_cb_t)(screen_device_info_t *view,
                                                void *user_data);
typedef void (*screen_device_info_hidden_cb_t)(screen_device_info_t *view,
                                               void *user_data);

typedef enum {
    SCREEN_DEVICE_INFO_UPDATE_IDLE = 0,
    SCREEN_DEVICE_INFO_UPDATE_CHECKING,
    SCREEN_DEVICE_INFO_UPDATE_UP_TO_DATE,
    SCREEN_DEVICE_INFO_UPDATE_AVAILABLE,
    SCREEN_DEVICE_INFO_UPDATE_STAGED,
    SCREEN_DEVICE_INFO_UPDATE_CHECK_FAILED,
} screen_device_info_update_state_t;

typedef struct {
    bool wifi_connected;
    char wifi_ssid[33];
    char wifi_ip[16];
    bool ap_enabled;
    char ap_ssid[33];
    char ap_ip[16];
    char firmware_version[64];
    char time_text[6];
} screen_device_info_data_t;

/**
 * Create the device-information page.
 *
 * The page is created hidden. Use screen_device_info_show() to display it.
 * Its LVGL objects are owned by parent and are deleted with parent.
 */
screen_device_info_t *screen_device_info_create(lv_obj_t *parent);

/** Release the component handle. The parent owns the LVGL object tree. */
void screen_device_info_destroy(screen_device_info_t *view);

/** Return the root LVGL object of the device-information page. */
lv_obj_t *screen_device_info_get_obj(screen_device_info_t *view);

/** Apply a complete device-information snapshot to the page. */
void screen_device_info_set_data(screen_device_info_t *view,
                                 const screen_device_info_data_t *data);

/** Configure the action invoked when the Check Update button is pressed. */
void screen_device_info_set_check_update_cb(
    screen_device_info_t *view,
    screen_device_info_check_update_cb_t callback,
    void *user_data);

/** Configure the action invoked when the available-version Upgrade link is pressed. */
void screen_device_info_set_upgrade_cb(screen_device_info_t *view,
                                       screen_device_info_upgrade_cb_t callback,
                                       void *user_data);

/** Configure the action invoked after the hide animation has completed. */
void screen_device_info_set_hidden_cb(screen_device_info_t *view,
                                      screen_device_info_hidden_cb_t callback,
                                      void *user_data);

/** Switch the update button between its idle and animated checking styles. */
void screen_device_info_set_checking(screen_device_info_t *view, bool checking);

/** Render the firmware-check result in the Version card. */
void screen_device_info_set_update_state(
    screen_device_info_t *view,
    screen_device_info_update_state_t state,
    const char *available_version);

/** Show or hide the page with a vertical slide animation. */
void screen_device_info_show(screen_device_info_t *view);
void screen_device_info_hide(screen_device_info_t *view);

/** Return true when the page exists and is currently visible. */
bool screen_device_info_is_visible(const screen_device_info_t *view);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_SCREEN_DEVICE_INFO_H
