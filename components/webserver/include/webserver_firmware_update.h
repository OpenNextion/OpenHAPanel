#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WEBSERVER_FIRMWARE_UPDATE_SOURCE_NONE = 0,
    WEBSERVER_FIRMWARE_UPDATE_SOURCE_SCREEN,
    WEBSERVER_FIRMWARE_UPDATE_SOURCE_AUTOMATIC,
} webserver_firmware_update_source_t;

/**
 * Update the cached OTA status and notify authenticated WebSocket clients when
 * a meaningful field changes. Progress notifications are throttled to 5% steps
 * while the Web Console is blocked.
 */
esp_err_t webserver_firmware_update_status_set(
    bool active,
    bool operation_blocked,
    webserver_firmware_update_source_t source,
    const char *state,
    uint8_t progress,
    const char *target_version);

/** Send the current cached OTA status to authenticated WebSocket clients. */
esp_err_t webserver_firmware_update_status_notify(void);

/** Return whether normal WebSocket API operations are currently blocked. */
bool webserver_firmware_update_operations_blocked(void);

/** Return whether a method must remain usable while OTA operations are blocked. */
bool webserver_firmware_update_method_allowed(const char *method);

#ifdef __cplusplus
}
#endif
