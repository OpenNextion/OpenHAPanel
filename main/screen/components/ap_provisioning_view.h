#ifndef OPEN_NEXTION_MAIN_SCREEN_AP_PROVISIONING_VIEW_H
#define OPEN_NEXTION_MAIN_SCREEN_AP_PROVISIONING_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *ssid;
    const char *setup_url;
    const char *credential_text;
} ap_provisioning_view_config_t;

/** Create the AP provisioning page as a full-size child of parent. */
lv_obj_t *ap_provisioning_view_create(lv_obj_t *parent,
                                      const ap_provisioning_view_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_AP_PROVISIONING_VIEW_H
