#include <esp_log.h>
#include <time.h>
#include <esp_wifi.h>
#include "sys_time.h"
#include "sys_time_api.h"
#include "webserver_api.h"
#include "webserver_config_api.h"
#include <webserver_client_node.h>
#include "littlefs.h"
#include "firmware_auto_check.h"
#include "firmware_update.h"
#include "ha_auth.h"
#include "login_session.h"
#include "screen_state.h"
#include "ui_preferences.h"

#ifndef CONFIG_FIRMWARE_VERSION_STRING
#ifdef CONFIG_APP_PROJECT_VER
#define CONFIG_FIRMWARE_VERSION_STRING CONFIG_APP_PROJECT_VER
#else
#define CONFIG_FIRMWARE_VERSION_STRING "1.0.0"
#endif
#endif

#ifndef CONFIG_DEVICE_MODEL_STRING
#define CONFIG_DEVICE_MODEL_STRING "ONX3248G035"
#endif

/**
 * @brief Logger tag
 */
static const char *TAG = "webserver_config_api";

extern webserver_config_t config_info;
extern device_info_t device_info;
extern night_mode_config_t s_night_mode_config;

/**
 * @brief Get the pointer to the global configuration structure.
 *
 * @return Pointer to the Config structure holding the current configuration.
 */
webserver_config_t *webserver_config_get_config(void)
{
    return &config_info;
}

/**
 * @brief Get the pointer to the global device information structure.
 *
 * @return Pointer to the DeviceInfo structure holding device information.
 */
device_info_t *webserver_config_get_device_info(void)
{
    return &device_info;
}

night_mode_config_t *webserver_config_get_night_mode_config(void)
{
    return &s_night_mode_config;
}

/**
 * @brief Notify clients about the current IP whitelist configuration.
 *
 * Creates a JSON object containing the IP whitelist enabled state and the list of IPs,
 * then sends a notification with the "Security.InfoNotify" event.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
int webserver_whitelist_notify()
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        ESP_LOGE(TAG, "Failed to create data JSON");
        return ESP_FAIL;
    }

    cJSON_AddBoolToObject(data, "ipWhitelistEnabled", config_info.enabled);

    cJSON *ip_list = cJSON_CreateArray();
    if (!ip_list) {
        ESP_LOGE(TAG, "Failed to create IP whitelist array");
        cJSON_Delete(data);
        return ESP_FAIL;
    }

    for (int i = 0; i < config_info.ip_count; i++) {
        cJSON_AddItemToArray(ip_list, cJSON_CreateString(config_info.ip_White_list[i]));
    }
    cJSON_AddItemToObject(data, "ipWhitelist", ip_list);
    webserver_data_response_to_all_client(data, WEB_API_METHOD_SECU_INFO_NOTIFY, 0);
    return ESP_OK;
}

/**
 * @brief Notify clients about the current theme mode configuration.
 *
 * Creates a JSON object with the current theme mode and follow system flag,
 * then sends a notification with the "Device.ThemeModeNotify" event.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
int webserver_theme_mode_notify()
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        ESP_LOGE(TAG, "Failed to create data object");
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(data, "currentMode", device_info.theme_mode.current_mode);
    cJSON_AddBoolToObject(data, "followSystem", device_info.theme_mode.follow_system);
    webserver_data_response_to_all_client(data, WEB_API_METHOD_THEME_MODE_NOTIFY, 0);
    return ESP_OK;
}

/**
 * @brief Notify clients about the current language configuration.
 *
 * Creates a JSON object with the current language and sends a notification with the "Device.LanguageNotify" event.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
int webserver_language_notify()
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        ESP_LOGE(TAG, "Failed to create data object");
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(data, "language", device_info.language);
    webserver_data_response_to_all_client(data, WEB_API_METHOD_LANGUAGE_NOTIFY, 0);
    return ESP_OK;
}

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
int webserver_device_info_notify(uint8_t update_info)
{
    time_t now = time(NULL);
    int64_t timestamp_ms = now * 1000;
    double boot_time_stamp = esp_log_timestamp() / 1000;

    char ntp_server_main[NTP_DOMAIN_MAX_LEN] = "pool.ntp.org";
    char ntp_server_spare[NTP_DOMAIN_MAX_LEN] = "time.google.com";
    timezone_info_t tz_info;
    sys_time_get_time_info(&tz_info);
    sys_time_get_ntp_server_name(ntp_server_main, ntp_server_spare);

    /* 创建 data 对象 */
    cJSON *data = cJSON_CreateObject();
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_NAME) {
        cJSON_AddStringToObject(data, "name", device_info.product_name);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_ID) {
        cJSON_AddStringToObject(data, "id", webserver_config_get_product_id());
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_BOOT_TIME) {
        cJSON_AddNumberToObject(data, "bootTimestamp", boot_time_stamp);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_CURRENT_TIME) {
        cJSON_AddNumberToObject(data, "currentTime", timestamp_ms);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_TIME_ZONE) {
        cJSON *tzInfo = cJSON_CreateObject();
        cJSON_AddStringToObject(tzInfo, "timeZone", tz_info.timezone);
        cJSON_AddNumberToObject(tzInfo, "utcOffset", tz_info.utc_office);
        cJSON_AddBoolToObject(tzInfo, "isDst", tz_info.is_dst);
        cJSON_AddBoolToObject(tzInfo, "hasDst", tz_info.has_dst);
        cJSON_AddNumberToObject(tzInfo, "nextDstShift", tz_info.next_dst ? (tz_info.next_dst * 1000) : 0);
        cJSON_AddNumberToObject(tzInfo, "nextUtcOffset", tz_info.next_utc_office ? tz_info.next_utc_office : 0);
        cJSON_AddItemToObject(data, "tzInfo", tzInfo);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_NTP_SERVER) {
        cJSON *ntpServers = cJSON_CreateObject();
        cJSON_AddStringToObject(ntpServers, "server1", ntp_server_main);
        cJSON_AddStringToObject(ntpServers, "server2", ntp_server_spare);
        cJSON_AddItemToObject(data, "ntpServers", ntpServers);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_LANGUAGE) {
        cJSON_AddStringToObject(data, "language", device_info.language);
        cJSON *esp32 = cJSON_CreateObject();
        cJSON_AddStringToObject(esp32, "model", CONFIG_DEVICE_MODEL_STRING);
        cJSON_AddStringToObject(esp32, "currentVersion", CONFIG_FIRMWARE_VERSION_STRING);
        cJSON_AddItemToObject(data, "esp32", esp32);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_BRIGHTNESS) {
        cJSON_AddNumberToObject(data, "brightness", device_info.brightness);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_IDLE_TIME) {
        cJSON_AddNumberToObject(data, "idleTime", device_info.idle_time);
    }
    if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_NIGHT_MODE) {
        cJSON *nightMode = cJSON_CreateObject();
        cJSON_AddBoolToObject(nightMode, "enable", s_night_mode_config.enable);
        cJSON_AddNumberToObject(nightMode, "brightness", s_night_mode_config.brightness);
        cJSON_AddStringToObject(nightMode, "from", s_night_mode_config.from);
        cJSON_AddStringToObject(nightMode, "to", s_night_mode_config.to);
        cJSON_AddItemToObject(data, "nightMode", nightMode);
    }
    // if (update_info == UPDATE_DEVICE_ALL || update_info == UPDATE_IEEE) {
    //     wireless_controller_ieee_info_t *ieee_info = wireless_controller_get_ieee_info();
    //     cJSON *ieee_address = cJSON_CreateObject();
    //     if (ieee_info->custom_eui64[0] != '\0') {
    //         char custom_addr_with_prefix[32];
    //         snprintf(custom_addr_with_prefix, sizeof(custom_addr_with_prefix), "0x%s", ieee_info->custom_eui64);
    //         cJSON_AddStringToObject(ieee_address, "customAddress", custom_addr_with_prefix);
    //     }
    //     char factory_addr_with_prefix[32];
    //     snprintf(factory_addr_with_prefix, sizeof(factory_addr_with_prefix), "0x%s", ieee_info->factory_eui64);
    //     cJSON_AddStringToObject(ieee_address, "factoryAddress", factory_addr_with_prefix);
    //     cJSON_AddItemToObject(data, "IEEEAddress", ieee_address);
    // }

    webserver_data_response_to_all_client(data, WEB_API_METHOD_DEVICE_INFO_NOTIFY, 0);
    return ESP_OK;
}

/**
 * @brief Restore device factory settings and reboot.
 *
 * Deletes configuration files and factory-resettable NVS state, then reboots.
 */
void webserver_config_restore_factory_settings(void)
{
    ESP_LOGI(TAG, "RESTORE_FACTORY_SETTINGS");
    screen_state_request_transition(SCREEN_PAGE_RESTARTING);
    little_file_system_delete_all_config_file();
    // wireless_controller_reboot();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_err_t ota_schedule_erase_err = firmware_auto_check_prepare_factory_reset();
    if (ota_schedule_erase_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to erase automatic OTA schedule: %s",
                 esp_err_to_name(ota_schedule_erase_err));
    }
    esp_err_t staged_firmware_erase_err = firmware_update_prepare_factory_reset();
    if (staged_firmware_erase_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to erase staged firmware metadata: %s",
                 esp_err_to_name(staged_firmware_erase_err));
    }
    esp_err_t ha_auth_erase_err = ha_auth_prepare_factory_reset();
    if (ha_auth_erase_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase Home Assistant authorization: %s", esp_err_to_name(ha_auth_erase_err));
    }
    esp_err_t login_session_erase_err = login_session_prepare_factory_reset();
    if (login_session_erase_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase remembered login sessions: %s", esp_err_to_name(login_session_erase_err));
    }
    esp_err_t ui_preferences_erase_err = ui_preferences_prepare_factory_reset();
    if (ui_preferences_erase_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to erase UI preferences: %s",
                 esp_err_to_name(ui_preferences_erase_err));
    }
    // notification_manage_restore_factory_settings();
    webserver_force_free_all_clients();
    // notification_manage_esp32_reboot_notify_tigger();
    esp_restart();
}
