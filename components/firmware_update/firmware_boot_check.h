#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "firmware_update_types.h"

typedef enum {
    FIRMWARE_BOOT_CHECK_NORMAL = 0,
    FIRMWARE_BOOT_CHECK_PENDING,
    FIRMWARE_BOOT_CHECK_ROLLED_BACK,
} firmware_boot_check_state_t;

typedef struct {
    firmware_boot_check_state_t state;
    esp_err_t last_error;
    char target_version[FIRMWARE_UPDATE_VERSION_MAX_LEN];
} firmware_boot_check_result_t;

esp_err_t firmware_boot_check_inspect(firmware_boot_check_result_t *result);
esp_err_t firmware_boot_check_confirm(bool *image_confirmed);
esp_err_t firmware_boot_check_rollback_and_reboot(esp_err_t error);
