#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FIRMWARE_AUTO_CHECK_RESULT_NONE = 0,
    FIRMWARE_AUTO_CHECK_RESULT_STARTED,
    FIRMWARE_AUTO_CHECK_RESULT_UP_TO_DATE,
    FIRMWARE_AUTO_CHECK_RESULT_UPDATE_AVAILABLE,
    FIRMWARE_AUTO_CHECK_RESULT_CHECK_FAILED,
    FIRMWARE_AUTO_CHECK_RESULT_DOWNLOADING,
    FIRMWARE_AUTO_CHECK_RESULT_STAGED,
    FIRMWARE_AUTO_CHECK_RESULT_DOWNLOAD_FAILED,
} firmware_auto_check_result_t;

typedef struct {
    bool enabled;
    uint32_t last_check_time;
    uint32_t last_daily_check_local_day;
    firmware_auto_check_result_t last_result;
    esp_err_t last_error;
    uint32_t scheduled_local_day;
    uint16_t scheduled_minute;
} firmware_auto_check_record_t;

/** Initialize the Wi-Fi-connect and daily local-time update checker. */
esp_err_t firmware_auto_check_init(void);

/** Record a new Wi-Fi STA association. */
void firmware_auto_check_notify_wifi_connected(void);

/** Schedule a Manifest check after the current Wi-Fi association obtains IPv4. */
void firmware_auto_check_notify_wifi_got_ip(void);

/** Cancel a pending stability window after Wi-Fi STA disconnects. */
void firmware_auto_check_notify_wifi_disconnected(void);

/** Read the persistent automatic-check schedule and most recent result. */
esp_err_t firmware_auto_check_get_record(firmware_auto_check_record_t *record);

/** Persistently enable or disable Wi-Fi-connect and daily automatic checks. */
esp_err_t firmware_auto_check_set_enabled(bool enabled);

/** Stop the scheduler and erase its persistent state before a factory reset. */
esp_err_t firmware_auto_check_prepare_factory_reset(void);

/**
 * Return true when the current AVAILABLE firmware was discovered by the
 * Manifest-only Wi-Fi-connect check and should appear on Device Info.
 */
bool firmware_auto_check_has_wifi_candidate(void);

#ifdef __cplusplus
}
#endif
