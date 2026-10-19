#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_SCREEN_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_SCREEN_H

#include "webserver.h"

/**
 * @brief Initialize the screen lock timer.
 *
 * This function sets up the timer to lock the screen after a period of inactivity.
 * The idle time is obtained from the device configuration.
 *
 */
void screen_lock_init(void);

/**
 * @brief Set the screen lock idle time.
 *
 * This function sets the idle time for the screen to lock after inactivity.
 * The idle time is specified in seconds.
 *
 * @param idle_time The idle time in seconds for the screen to lock.
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error.
 */
esp_err_t screen_set_lock(int idle_time, uint8_t brightness);

/**
 * @brief Reset the screen lock timer.
 *
 * This function resets the screen lock timer to start a new period of inactivity.
 * The timer is used to lock the screen after a period of inactivity.
 *
 */
void screen_lock_reset_timer(void);

bool screen_is_locked(void);

/** Apply an explicit HA screen lock/unlock command. Touch may unlock it later. */
esp_err_t screen_set_ha_lock(bool locked);
/** Toggle the physical screen between dark and the configured brightness. */
esp_err_t screen_toggle_power(void);
/** Apply a HA brightness command and clear the explicit/idle lock. */
esp_err_t screen_set_ha_brightness(uint8_t brightness);

#endif 

#ifdef __cplusplus
}
#endif
