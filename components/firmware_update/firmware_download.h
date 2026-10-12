#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_partition.h"
#include "firmware_manifest.h"
#include "firmware_update_types.h"

typedef enum {
    FIRMWARE_DOWNLOAD_STAGE_DOWNLOADING = 0,
    FIRMWARE_DOWNLOAD_STAGE_VERIFYING,
} firmware_download_stage_t;

typedef void (*firmware_download_progress_cb_t)(firmware_download_stage_t stage,
                                                uint8_t progress,
                                                void *user_context);

/**
 * @brief Download and validate the Manifest image without activating it.
 *
 * On success, update_partition contains a complete image but the configured
 * boot partition is unchanged.
 */
esp_err_t firmware_download_stage(const firmware_manifest_t *manifest,
                                  const esp_partition_t *update_partition,
                                  firmware_download_progress_cb_t progress_cb,
                                  void *user_context);

/** Validate a previously staged image against its persistent metadata. */
esp_err_t firmware_download_validate_staged(
    const firmware_update_staged_info_t *staged,
    const esp_partition_t *partition);
