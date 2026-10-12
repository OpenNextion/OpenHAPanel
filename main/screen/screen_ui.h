#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_MAIN_SCREEN_SCREEN_UI_H
#define OPEN_NEXTION_MAIN_SCREEN_SCREEN_UI_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "ha_entity_store_type.h"
#include "lvgl.h"
#include "screen_config.h"
#include "screen_state.h"

typedef enum {
    SCREEN_NOTIFICATION_HA_DISCONNECTED = 1,
    SCREEN_NOTIFICATION_HA_AUTH_CANCELED,
    SCREEN_NOTIFICATION_HA_AUTH_EXPIRED,
    SCREEN_NOTIFICATION_WIFI_DISCONNECTED,
    SCREEN_NOTIFICATION_FIRMWARE_UPDATE,
    SCREEN_NOTIFICATION_COUNT,
} screen_notification_type_t;

/** Register the screen-state transition-request listener. Idempotent. */
esp_err_t screen_ui_init(void);
/** Load and stage configuration, then request scheduling through the state handler. */
esp_err_t screen_ui_reload(void);
void clear_lvgl_screen(void);

void screen_ui_set_status_title(const char *title);
void screen_ui_set_status_time(const char *time_str);
/** Show or hide the new-firmware indicator in every system status bar. */
void screen_ui_set_ota_update_available(bool available);
/** Show or hide the Home Assistant connected indicator in every system status bar. */
void screen_ui_set_ha_connected(bool connected);
void screen_ui_set_active_screen(int32_t screen_index);

/** Show or hide the static device-information page. */
void screen_ui_show_device_info(void);
void screen_ui_hide_device_info(void);

/**
 * Activate or clear a persistent system notification condition.
 * Call while holding the LVGL lock. Repeated activation is deduplicated.
 */
void screen_ui_set_notification_active(screen_notification_type_t type, bool active);

/**
 * Schedule or cancel the transient Wi-Fi-connected Toast.
 * Call while holding the LVGL lock. A scheduled Toast is shown after the HA
 * provisioning page has been created.
 */
void screen_ui_set_wifi_connected_toast_pending(bool pending);

bool screen_ui_update_badge(int32_t screen_index,
                            size_t badge_index,
                            const char *name,
                            const char *state);
size_t screen_ui_update_entity(int32_t screen_index,
                               const char *entity_id,
                               const char *name,
                               const char *state);
size_t screen_ui_update_entity_all(const char *entity_id,
                                   const char *name,
                                   const char *state);
size_t screen_ui_refresh_entity_all(ha_entity_store_t *store,
                                    const char *entity_id);

/** Rebind every rendered tile to store. Pass NULL to release all subscriptions. */
void screen_ui_set_entity_store(ha_entity_store_t *store);

#endif // OPEN_NEXTION_MAIN_SCREEN_SCREEN_UI_H

#ifdef __cplusplus
}
#endif
