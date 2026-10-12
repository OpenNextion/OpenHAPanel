#include "firmware_boot_check.h"

#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "firmware_storage.h"

static const char *TAG = "firmware_boot_check";

static bool firmware_boot_check_partition_has_version(const esp_partition_t *partition,
                                                      const char *version)
{
    if (!partition || !version || version[0] == '\0') {
        return false;
    }

    esp_app_desc_t description = {0};
    if (esp_ota_get_partition_description(partition, &description) != ESP_OK) {
        return false;
    }
    return strnlen(description.version, sizeof(description.version)) == strlen(version) &&
           strncmp(description.version, version, sizeof(description.version)) == 0;
}

esp_err_t firmware_boot_check_inspect(firmware_boot_check_result_t *result)
{
    if (!result) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(result, 0, sizeof(*result));

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (!running_partition) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_img_states_t image_state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t state_err = esp_ota_get_state_partition(running_partition, &image_state);
    if (state_err != ESP_OK && state_err != ESP_ERR_NOT_FOUND) {
        return state_err;
    }

    const esp_app_desc_t *running_description = esp_app_get_description();
    if (!running_description) {
        return ESP_ERR_NOT_FOUND;
    }

    firmware_update_history_t history;
    esp_err_t storage_err = firmware_storage_get_history(&history);
    bool history_available = storage_err == ESP_OK;
    if (!history_available) {
        ESP_LOGW(TAG, "OTA history is unavailable: %s", esp_err_to_name(storage_err));
    }

    if (state_err == ESP_OK && image_state == ESP_OTA_IMG_PENDING_VERIFY) {
        result->state = FIRMWARE_BOOT_CHECK_PENDING;
        strlcpy(result->target_version,
                running_description->version,
                sizeof(result->target_version));

        if (!history_available ||
            history.last_result != FIRMWARE_UPDATE_RESULT_PENDING ||
            strcmp(history.pending_version, running_description->version) != 0) {
            const char *previous_version = history_available &&
                                           history.last_good_version[0] != '\0'
                                               ? history.last_good_version
                                               : running_description->version;
            esp_err_t err = firmware_storage_begin_update(previous_version,
                                                          running_description->version);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to repair pending OTA history: %s", esp_err_to_name(err));
            }
        }

        ESP_LOGW(TAG,
                 "Running image requires first-boot verification: version=%s partition=%s",
                 running_description->version,
                 running_partition->label);
        return ESP_OK;
    }

    if (!history_available || history.last_result != FIRMWARE_UPDATE_RESULT_PENDING) {
        result->state = FIRMWARE_BOOT_CHECK_NORMAL;
        return ESP_OK;
    }

    strlcpy(result->target_version,
            history.pending_version,
            sizeof(result->target_version));
    const esp_partition_t *last_invalid = esp_ota_get_last_invalid_partition();
    if (firmware_boot_check_partition_has_version(last_invalid,
                                                  history.pending_version)) {
        result->state = FIRMWARE_BOOT_CHECK_ROLLED_BACK;
        result->last_error = history.last_error == ESP_OK ? ESP_FAIL : history.last_error;
        esp_err_t err = firmware_storage_record_rollback(running_description->version,
                                                         result->last_error);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save OTA rollback history: %s", esp_err_to_name(err));
        }
        ESP_LOGW(TAG,
                 "Detected OTA rollback: failed_version=%s running_version=%s",
                 result->target_version,
                 running_description->version);
        return ESP_OK;
    }

    if (strcmp(history.pending_version, running_description->version) == 0) {
        esp_err_t err = firmware_storage_record_boot_success(running_description->version);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to repair successful OTA history: %s", esp_err_to_name(err));
        }
    }
    else {
        esp_err_t err = firmware_storage_record_install_failure(ESP_ERR_INVALID_STATE);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to record interrupted OTA installation: %s", esp_err_to_name(err));
        }
    }
    result->state = FIRMWARE_BOOT_CHECK_NORMAL;
    return ESP_OK;
}

esp_err_t firmware_boot_check_confirm(bool *image_confirmed)
{
    if (!image_confirmed) {
        return ESP_ERR_INVALID_ARG;
    }
    *image_confirmed = false;

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (!running_partition) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_img_states_t image_state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t err = esp_ota_get_state_partition(running_partition, &image_state);
    if (err == ESP_ERR_NOT_FOUND || (err == ESP_OK && image_state != ESP_OTA_IMG_PENDING_VERIFY)) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        return err;
    }
    *image_confirmed = true;

    const esp_app_desc_t *description = esp_app_get_description();
    if (!description) {
        return ESP_ERR_NOT_FOUND;
    }
    err = firmware_storage_record_boot_success(description->version);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Image is valid but success history could not be saved: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA image confirmed as valid: version=%s", description->version);
    return ESP_OK;
}

esp_err_t firmware_boot_check_rollback_and_reboot(esp_err_t error)
{
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (!running_partition) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_img_states_t image_state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t err = esp_ota_get_state_partition(running_partition, &image_state);
    if (err != ESP_OK) {
        return err;
    }
    if (image_state != ESP_OTA_IMG_PENDING_VERIFY) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t storage_err = firmware_storage_record_boot_failure(error);
    if (storage_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save boot self-test failure: %s", esp_err_to_name(storage_err));
    }
    ESP_LOGE(TAG, "Boot self-test failed; rolling back: %s", esp_err_to_name(error));
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}
