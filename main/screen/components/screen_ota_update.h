#ifndef OPEN_NEXTION_MAIN_SCREEN_SCREEN_OTA_UPDATE_H
#define OPEN_NEXTION_MAIN_SCREEN_SCREEN_OTA_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct screen_ota_update screen_ota_update_t;
typedef void (*screen_ota_update_action_cb_t)(screen_ota_update_t *view,
                                              void *user_data);

screen_ota_update_t *screen_ota_update_create(lv_obj_t *parent);
void screen_ota_update_destroy(screen_ota_update_t *view);

void screen_ota_update_set_callbacks(screen_ota_update_t *view,
                                     screen_ota_update_action_cb_t retry_cb,
                                     screen_ota_update_action_cb_t cancel_cb,
                                     void *user_data);

void screen_ota_update_show_updating(screen_ota_update_t *view, uint8_t progress);
void screen_ota_update_show_failed(screen_ota_update_t *view);
void screen_ota_update_show_restarting(screen_ota_update_t *view);
void screen_ota_update_hide(screen_ota_update_t *view);
bool screen_ota_update_is_visible(const screen_ota_update_t *view);
void screen_ota_update_bring_to_front(screen_ota_update_t *view);

#ifdef __cplusplus
}
#endif

#endif
