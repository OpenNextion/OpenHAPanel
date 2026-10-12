#ifndef OPEN_NEXTION_MAIN_SCREEN_HA_PROVISIONING_VIEW_H
#define OPEN_NEXTION_MAIN_SCREEN_HA_PROVISIONING_VIEW_H

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ha_provisioning_hint_dismissed_cb_t)(void *user_data);

typedef struct {
    bool show_device_info_hint;
    ha_provisioning_hint_dismissed_cb_t hint_dismissed_cb;
    void *hint_dismissed_user_data;
} ha_provisioning_view_config_t;

/** Create the Home Assistant provisioning guide as a full-size child of parent. */
lv_obj_t *ha_provisioning_view_create(
    lv_obj_t *parent,
    const ha_provisioning_view_config_t *config);

/** Dismiss the device-information swipe hint, if it is pending or visible. */
void ha_provisioning_view_dismiss_hint(lv_obj_t *root);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_HA_PROVISIONING_VIEW_H
