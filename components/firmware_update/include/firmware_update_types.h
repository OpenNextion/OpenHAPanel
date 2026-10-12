#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIRMWARE_UPDATE_VERSION_MAX_LEN 32
#define FIRMWARE_UPDATE_PARTITION_LABEL_MAX_LEN 17
#define FIRMWARE_UPDATE_PUBLISHED_AT_MAX_LEN 32

typedef enum {
    FIRMWARE_UPDATE_STATE_IDLE = 0,
    FIRMWARE_UPDATE_STATE_CHECKING,
    FIRMWARE_UPDATE_STATE_UP_TO_DATE,
    FIRMWARE_UPDATE_STATE_AVAILABLE,
    FIRMWARE_UPDATE_STATE_DOWNLOADING,
    FIRMWARE_UPDATE_STATE_VERIFYING,
    FIRMWARE_UPDATE_STATE_STAGED,
    FIRMWARE_UPDATE_STATE_ACTIVATING,
    FIRMWARE_UPDATE_STATE_READY_TO_REBOOT,
    FIRMWARE_UPDATE_STATE_REBOOTING,
    FIRMWARE_UPDATE_STATE_BOOT_VERIFYING,
    FIRMWARE_UPDATE_STATE_SUCCESS,
    FIRMWARE_UPDATE_STATE_ROLLED_BACK,
    FIRMWARE_UPDATE_STATE_FAILED,
} firmware_update_state_t;

typedef struct {
    firmware_update_state_t state;
    uint8_t progress;
    esp_err_t last_error;
    uint32_t revision;
    char current_version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
    char target_version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
} firmware_update_status_t;

typedef struct {
    bool valid;
    bool automatic;
    uint32_t image_size;
    uint8_t image_sha256[32];
    char version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
    char published_at[FIRMWARE_UPDATE_PUBLISHED_AT_MAX_LEN];
    char partition_label[FIRMWARE_UPDATE_PARTITION_LABEL_MAX_LEN];
} firmware_update_staged_info_t;

typedef enum {
    FIRMWARE_UPDATE_RESULT_NONE = 0,
    FIRMWARE_UPDATE_RESULT_PENDING,
    FIRMWARE_UPDATE_RESULT_SUCCESS,
    FIRMWARE_UPDATE_RESULT_FAILED,
    FIRMWARE_UPDATE_RESULT_ROLLED_BACK,
} firmware_update_result_t;

typedef struct {
    firmware_update_result_t last_result;
    esp_err_t last_error;
    bool rollback_notice;
    char pending_version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
    char target_version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
    char previous_version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
    char last_good_version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
} firmware_update_history_t;

#ifdef __cplusplus
}
#endif
