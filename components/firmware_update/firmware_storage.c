#include "firmware_storage.h"

#include <string.h>

#include "nvs_storage.h"

#define FIRMWARE_STORAGE_NAMESPACE "ota"
#define FIRMWARE_STORAGE_SCHEMA_VERSION 1

#define FIRMWARE_STORAGE_KEY_SCHEMA "schema"
#define FIRMWARE_STORAGE_KEY_RESULT "last_result"
#define FIRMWARE_STORAGE_KEY_ERROR "last_error"
#define FIRMWARE_STORAGE_KEY_PENDING "pending_ver"
#define FIRMWARE_STORAGE_KEY_TARGET "target_ver"
#define FIRMWARE_STORAGE_KEY_PREVIOUS "previous_ver"
#define FIRMWARE_STORAGE_KEY_LAST_GOOD "good_ver"
#define FIRMWARE_STORAGE_KEY_ROLLBACK_NOTICE "rollback_notice"

#define FIRMWARE_STAGED_NAMESPACE "ota_stage"
#define FIRMWARE_STAGED_SCHEMA_VERSION 2
#define FIRMWARE_STAGED_KEY_SCHEMA "schema"
#define FIRMWARE_STAGED_KEY_VALID "valid"
#define FIRMWARE_STAGED_KEY_AUTOMATIC "automatic"
#define FIRMWARE_STAGED_KEY_VERSION "version"
#define FIRMWARE_STAGED_KEY_PUBLISHED_AT "published_at"
#define FIRMWARE_STAGED_KEY_PARTITION "partition"
#define FIRMWARE_STAGED_KEY_SIZE "image_size"
#define FIRMWARE_STAGED_KEY_SHA256 "sha256"
#define FIRMWARE_STAGED_SHA256_HEX_LENGTH 64

static void firmware_storage_set_string(char *destination,
                                        size_t destination_size,
                                        const char *source)
{
    strlcpy(destination, source ? source : "", destination_size);
}

static void firmware_storage_sha256_to_hex(const uint8_t sha256[32],
                                           char output[FIRMWARE_STAGED_SHA256_HEX_LENGTH + 1])
{
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < 32; index++) {
        output[index * 2] = digits[sha256[index] >> 4];
        output[index * 2 + 1] = digits[sha256[index] & 0x0f];
    }
    output[FIRMWARE_STAGED_SHA256_HEX_LENGTH] = '\0';
}

static int firmware_storage_hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool firmware_storage_sha256_from_hex(const char *input,
                                             uint8_t output[32])
{
    if (!input || strlen(input) != FIRMWARE_STAGED_SHA256_HEX_LENGTH) {
        return false;
    }
    for (size_t index = 0; index < 32; index++) {
        int high = firmware_storage_hex_value(input[index * 2]);
        int low = firmware_storage_hex_value(input[index * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        output[index] = (uint8_t)((high << 4) | low);
    }
    return true;
}

static esp_err_t firmware_storage_write_history(const firmware_update_history_t *history)
{
    const nvs_storage_write_item_t items[] = {
        {
            .key = FIRMWARE_STORAGE_KEY_SCHEMA,
            .type = NVS_STORAGE_TYPE_U32,
            .value.u32 = FIRMWARE_STORAGE_SCHEMA_VERSION,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_RESULT,
            .type = NVS_STORAGE_TYPE_U8,
            .value.u8 = (uint8_t)history->last_result,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_ERROR,
            .type = NVS_STORAGE_TYPE_U32,
            .value.u32 = (uint32_t)history->last_error,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_PENDING,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = history->pending_version,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_TARGET,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = history->target_version,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_PREVIOUS,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = history->previous_version,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_LAST_GOOD,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = history->last_good_version,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_ROLLBACK_NOTICE,
            .type = NVS_STORAGE_TYPE_U8,
            .value.u8 = history->rollback_notice ? 1 : 0,
        },
    };
    return nvs_storage_write(FIRMWARE_STORAGE_NAMESPACE,
                             items,
                             sizeof(items) / sizeof(items[0]));
}

static void firmware_storage_set_defaults(firmware_update_history_t *history,
                                          const char *running_version)
{
    memset(history, 0, sizeof(*history));
    history->last_result = FIRMWARE_UPDATE_RESULT_NONE;
    history->last_error = ESP_OK;
    firmware_storage_set_string(history->last_good_version,
                                sizeof(history->last_good_version),
                                running_version);
}

esp_err_t firmware_storage_init(const char *running_version)
{
    if (!running_version || running_version[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    firmware_update_history_t history;
    esp_err_t err = firmware_storage_get_history(&history);
    if (err == ESP_OK) {
        return ESP_OK;
    }
    if (err != ESP_ERR_NVS_NOT_FOUND &&
        err != ESP_ERR_NVS_INVALID_LENGTH &&
        err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    firmware_storage_set_defaults(&history, running_version);
    return firmware_storage_write_history(&history);
}

esp_err_t firmware_storage_get_history(firmware_update_history_t *history)
{
    if (!history) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(history, 0, sizeof(*history));
    uint32_t schema_version = 0;
    uint8_t result = 0;
    uint32_t last_error = 0;
    uint8_t rollback_notice = 0;
    nvs_storage_read_item_t items[] = {
        {
            .key = FIRMWARE_STORAGE_KEY_SCHEMA,
            .type = NVS_STORAGE_TYPE_U32,
            .value = &schema_version,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_RESULT,
            .type = NVS_STORAGE_TYPE_U8,
            .value = &result,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_ERROR,
            .type = NVS_STORAGE_TYPE_U32,
            .value = &last_error,
        },
        {
            .key = FIRMWARE_STORAGE_KEY_PENDING,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = history->pending_version,
            .value_size = sizeof(history->pending_version),
        },
        {
            .key = FIRMWARE_STORAGE_KEY_TARGET,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = history->target_version,
            .value_size = sizeof(history->target_version),
        },
        {
            .key = FIRMWARE_STORAGE_KEY_PREVIOUS,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = history->previous_version,
            .value_size = sizeof(history->previous_version),
        },
        {
            .key = FIRMWARE_STORAGE_KEY_LAST_GOOD,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = history->last_good_version,
            .value_size = sizeof(history->last_good_version),
        },
        {
            .key = FIRMWARE_STORAGE_KEY_ROLLBACK_NOTICE,
            .type = NVS_STORAGE_TYPE_U8,
            .value = &rollback_notice,
        },
    };

    esp_err_t err = nvs_storage_read(FIRMWARE_STORAGE_NAMESPACE,
                                     items,
                                     sizeof(items) / sizeof(items[0]));
    if (err != ESP_OK) {
        return err;
    }
    for (size_t index = 0; index < sizeof(items) / sizeof(items[0]); index++) {
        if (items[index].result != ESP_OK) {
            return items[index].result;
        }
    }
    if (schema_version != FIRMWARE_STORAGE_SCHEMA_VERSION ||
        result > FIRMWARE_UPDATE_RESULT_ROLLED_BACK ||
        rollback_notice > 1) {
        return ESP_ERR_INVALID_STATE;
    }

    history->last_result = (firmware_update_result_t)result;
    history->last_error = (esp_err_t)last_error;
    history->rollback_notice = rollback_notice != 0;
    return ESP_OK;
}

esp_err_t firmware_storage_begin_update(const char *previous_version,
                                        const char *target_version)
{
    if (!previous_version || !target_version ||
        previous_version[0] == '\0' || target_version[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    firmware_update_history_t history;
    esp_err_t err = firmware_storage_get_history(&history);
    if (err != ESP_OK) {
        return err;
    }

    history.last_result = FIRMWARE_UPDATE_RESULT_PENDING;
    history.last_error = ESP_OK;
    history.rollback_notice = false;
    firmware_storage_set_string(history.previous_version,
                                sizeof(history.previous_version),
                                previous_version);
    firmware_storage_set_string(history.pending_version,
                                sizeof(history.pending_version),
                                target_version);
    firmware_storage_set_string(history.target_version,
                                sizeof(history.target_version),
                                target_version);
    return firmware_storage_write_history(&history);
}

esp_err_t firmware_storage_record_install_failure(esp_err_t error)
{
    firmware_update_history_t history;
    esp_err_t err = firmware_storage_get_history(&history);
    if (err != ESP_OK) {
        return err;
    }

    history.last_result = FIRMWARE_UPDATE_RESULT_FAILED;
    history.last_error = error;
    history.pending_version[0] = '\0';
    history.rollback_notice = false;
    return firmware_storage_write_history(&history);
}

esp_err_t firmware_storage_record_boot_failure(esp_err_t error)
{
    firmware_update_history_t history;
    esp_err_t err = firmware_storage_get_history(&history);
    if (err != ESP_OK) {
        return err;
    }
    if (history.last_result != FIRMWARE_UPDATE_RESULT_PENDING ||
        history.pending_version[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    history.last_error = error;
    return firmware_storage_write_history(&history);
}

esp_err_t firmware_storage_record_boot_success(const char *running_version)
{
    if (!running_version || running_version[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    firmware_update_history_t history;
    esp_err_t err = firmware_storage_get_history(&history);
    if (err != ESP_OK) {
        return err;
    }

    history.last_result = FIRMWARE_UPDATE_RESULT_SUCCESS;
    history.last_error = ESP_OK;
    history.pending_version[0] = '\0';
    history.rollback_notice = false;
    firmware_storage_set_string(history.last_good_version,
                                sizeof(history.last_good_version),
                                running_version);
    return firmware_storage_write_history(&history);
}

esp_err_t firmware_storage_record_rollback(const char *running_version,
                                           esp_err_t error)
{
    if (!running_version || running_version[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    firmware_update_history_t history;
    esp_err_t err = firmware_storage_get_history(&history);
    if (err != ESP_OK) {
        return err;
    }

    history.last_result = FIRMWARE_UPDATE_RESULT_ROLLED_BACK;
    history.last_error = error;
    history.pending_version[0] = '\0';
    history.rollback_notice = true;
    firmware_storage_set_string(history.last_good_version,
                                sizeof(history.last_good_version),
                                running_version);
    return firmware_storage_write_history(&history);
}

esp_err_t firmware_storage_clear_rollback_notice(void)
{
    firmware_update_history_t history;
    esp_err_t err = firmware_storage_get_history(&history);
    if (err != ESP_OK) {
        return err;
    }

    history.rollback_notice = false;
    return firmware_storage_write_history(&history);
}

esp_err_t firmware_storage_record_staged(const firmware_manifest_t *manifest,
                                         const esp_partition_t *partition,
                                         bool automatic)
{
    if (!manifest || !partition || manifest->version[0] == '\0' ||
        manifest->published_at[0] == '\0' ||
        manifest->firmware_size == 0 || manifest->firmware_size > partition->size) {
        return ESP_ERR_INVALID_ARG;
    }

    char sha256_hex[FIRMWARE_STAGED_SHA256_HEX_LENGTH + 1];
    firmware_storage_sha256_to_hex(manifest->firmware_sha256, sha256_hex);
    const nvs_storage_write_item_t items[] = {
        {
            .key = FIRMWARE_STAGED_KEY_SCHEMA,
            .type = NVS_STORAGE_TYPE_U32,
            .value.u32 = FIRMWARE_STAGED_SCHEMA_VERSION,
        },
        {
            .key = FIRMWARE_STAGED_KEY_VALID,
            .type = NVS_STORAGE_TYPE_U8,
            .value.u8 = 1,
        },
        {
            .key = FIRMWARE_STAGED_KEY_AUTOMATIC,
            .type = NVS_STORAGE_TYPE_U8,
            .value.u8 = automatic ? 1 : 0,
        },
        {
            .key = FIRMWARE_STAGED_KEY_VERSION,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = manifest->version,
        },
        {
            .key = FIRMWARE_STAGED_KEY_PUBLISHED_AT,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = manifest->published_at,
        },
        {
            .key = FIRMWARE_STAGED_KEY_PARTITION,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = partition->label,
        },
        {
            .key = FIRMWARE_STAGED_KEY_SIZE,
            .type = NVS_STORAGE_TYPE_U32,
            .value.u32 = manifest->firmware_size,
        },
        {
            .key = FIRMWARE_STAGED_KEY_SHA256,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = sha256_hex,
        },
    };
    return nvs_storage_write(FIRMWARE_STAGED_NAMESPACE,
                             items,
                             sizeof(items) / sizeof(items[0]));
}

esp_err_t firmware_storage_get_staged(firmware_update_staged_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(*info));
    uint32_t schema = 0;
    uint8_t valid = 0;
    uint8_t automatic = 0;
    char sha256_hex[FIRMWARE_STAGED_SHA256_HEX_LENGTH + 1] = {0};
    nvs_storage_read_item_t items[] = {
        {
            .key = FIRMWARE_STAGED_KEY_SCHEMA,
            .type = NVS_STORAGE_TYPE_U32,
            .value = &schema,
        },
        {
            .key = FIRMWARE_STAGED_KEY_VALID,
            .type = NVS_STORAGE_TYPE_U8,
            .value = &valid,
        },
        {
            .key = FIRMWARE_STAGED_KEY_AUTOMATIC,
            .type = NVS_STORAGE_TYPE_U8,
            .value = &automatic,
        },
        {
            .key = FIRMWARE_STAGED_KEY_VERSION,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = info->version,
            .value_size = sizeof(info->version),
        },
        {
            .key = FIRMWARE_STAGED_KEY_PUBLISHED_AT,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = info->published_at,
            .value_size = sizeof(info->published_at),
        },
        {
            .key = FIRMWARE_STAGED_KEY_PARTITION,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = info->partition_label,
            .value_size = sizeof(info->partition_label),
        },
        {
            .key = FIRMWARE_STAGED_KEY_SIZE,
            .type = NVS_STORAGE_TYPE_U32,
            .value = &info->image_size,
        },
        {
            .key = FIRMWARE_STAGED_KEY_SHA256,
            .type = NVS_STORAGE_TYPE_STRING,
            .value = sha256_hex,
            .value_size = sizeof(sha256_hex),
        },
    };

    esp_err_t err = nvs_storage_read(FIRMWARE_STAGED_NAMESPACE,
                                     items,
                                     sizeof(items) / sizeof(items[0]));
    if (err != ESP_OK) {
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : err;
    }
    for (size_t index = 0; index < sizeof(items) / sizeof(items[0]); index++) {
        if (items[index].result != ESP_OK) {
            return items[index].result == ESP_ERR_NVS_NOT_FOUND
                       ? ESP_ERR_NOT_FOUND
                       : items[index].result;
        }
    }
    if (schema != FIRMWARE_STAGED_SCHEMA_VERSION || valid != 1 || automatic > 1 ||
        info->version[0] == '\0' || info->published_at[0] == '\0' ||
        info->partition_label[0] == '\0' ||
        info->image_size == 0 ||
        !firmware_storage_sha256_from_hex(sha256_hex, info->image_sha256)) {
        return ESP_ERR_INVALID_STATE;
    }
    info->valid = true;
    info->automatic = automatic != 0;
    return ESP_OK;
}

esp_err_t firmware_storage_clear_staged(void)
{
    esp_err_t err = nvs_storage_erase_namespace(FIRMWARE_STAGED_NAMESPACE);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}
