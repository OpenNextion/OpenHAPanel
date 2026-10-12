#pragma once

#include "esp_err.h"
#include "esp_partition.h"
#include "firmware_manifest.h"
#include "firmware_update_types.h"

esp_err_t firmware_storage_init(const char *running_version);
esp_err_t firmware_storage_get_history(firmware_update_history_t *history);
esp_err_t firmware_storage_begin_update(const char *previous_version,
                                        const char *target_version);
esp_err_t firmware_storage_record_install_failure(esp_err_t error);
esp_err_t firmware_storage_record_boot_failure(esp_err_t error);
esp_err_t firmware_storage_record_boot_success(const char *running_version);
esp_err_t firmware_storage_record_rollback(const char *running_version,
                                           esp_err_t error);
esp_err_t firmware_storage_clear_rollback_notice(void);
esp_err_t firmware_storage_record_staged(const firmware_manifest_t *manifest,
                                         const esp_partition_t *partition,
                                         bool automatic);
esp_err_t firmware_storage_get_staged(firmware_update_staged_info_t *info);
esp_err_t firmware_storage_clear_staged(void);
