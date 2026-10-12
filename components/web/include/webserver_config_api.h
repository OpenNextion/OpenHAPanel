#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_API_H
#define OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_API_H

#include "webserver_config.h"

#define UPDATE_DEVICE_ALL 0   /* update all information */
#define UPDATE_NAME 1         /* update the product name */
#define UPDATE_ID 2           /* update the product id */
#define UPDATE_BOOT_TIME 3    /* update the device boot time */
#define UPDATE_CURRENT_TIME 4 /* update the current timestamp of the device */
#define UPDATE_TIME_ZONE 5    /* update the time zone */
#define UPDATE_NTP_SERVER 6   /* update the ntp server */
#define UPDATE_LANGUAGE 7     /* update-the-language */
#define UPDATE_IEEE 8         /* update-the-ieee */
#define UPDATE_BRIGHTNESS 9   /* update-the-brightness */
#define UPDATE_IDLE_TIME 10   /* update-the-idle-time */
#define UPDATE_NIGHT_MODE 11  /* update-the-night-mode */


/**
 * @brief Get the pointer to the global configuration structure.
 *
 * @return Pointer to the Config structure holding the current configuration.
 */
webserver_config_t *webserver_config_get_config(void);

/**
 * @brief Get the pointer to the global device information structure.
 *
 * @return Pointer to the DeviceInfo structure holding device information.
 */
device_info_t *webserver_config_get_device_info(void);

/**
 * @brief Notify clients about the current IP whitelist configuration.
 *
 * Creates a JSON object containing the IP whitelist enabled state and the list of IPs,
 * then sends a notification with the "Security.InfoNotify" event.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
int webserver_whitelist_notify(void);

/**
 * @brief Notify clients about the current theme mode configuration.
 *
 * Creates a JSON object with the current theme mode and follow system flag,
 * then sends a notification with the "Device.ThemeModeNotify" event.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
int webserver_theme_mode_notify(void);

/**
 * @brief Notify clients about the current language configuration.
 *
 * Creates a JSON object with the current language and sends a notification with the "Device.LanguageNotify" event.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
int webserver_language_notify(void);

/**
 * @brief Notify clients with device information updates.
 *
 * Sends a JSON notification with various device details based on the specified update flags.
 * The information can include product name, ID, boot time, current time, time zone info,
 * NTP server info, turbo mode state, and language.
 *
 * @param update_info Flags indicating which parts of device info to update (e.g. UPDATE_DEVICE_ALL).
 * @return ESP_OK always.
 */
int webserver_device_info_notify(uint8_t update_info);

/**
 * @brief Restore device factory settings and reboot.
 *
 * This function disconnects from Wi-Fi if connected, stops and restores the Wi-Fi driver,
 * deletes all config files, restores notifications and wireless_controller settings,
 * then reboots the device.
 */
void webserver_config_restore_factory_settings(void);

#endif // OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_API_H

#ifdef __cplusplus
}
#endif
