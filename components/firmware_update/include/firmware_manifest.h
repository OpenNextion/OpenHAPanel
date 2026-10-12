#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIRMWARE_MANIFEST_MAX_SIZE 8192
#define FIRMWARE_MANIFEST_SCHEMA_VERSION 2
#define FIRMWARE_MANIFEST_PRODUCT "open-ha-panel"
#define FIRMWARE_MANIFEST_CHIP "esp32s3"
#define FIRMWARE_MANIFEST_PARTITION_LAYOUT "ota-6m-v1"

#ifndef CONFIG_DEVICE_MODEL_STRING
#define CONFIG_DEVICE_MODEL_STRING "ONX3248G035"
#endif

#ifndef CONFIG_DEVICE_MODEL_SLUG
#define CONFIG_DEVICE_MODEL_SLUG "onx3248g035"
#endif

#define FIRMWARE_MANIFEST_PRODUCT_MAX_LEN 64
#define FIRMWARE_MANIFEST_CHANNEL_MAX_LEN 16
#define FIRMWARE_MANIFEST_VERSION_MAX_LEN 32
#define FIRMWARE_MANIFEST_TIMESTAMP_MAX_LEN 32
#define FIRMWARE_MANIFEST_GIT_SHA_LEN 40
#define FIRMWARE_MANIFEST_IDF_VERSION_MAX_LEN 24
#define FIRMWARE_MANIFEST_DEVICE_MODEL_MAX_LEN 32
#define FIRMWARE_MANIFEST_CHIP_MAX_LEN 16
#define FIRMWARE_MANIFEST_PARTITION_LAYOUT_MAX_LEN 32
#define FIRMWARE_MANIFEST_URL_MAX_LEN 512
#define FIRMWARE_MANIFEST_SHA256_HEX_LEN 64

typedef struct {
    uint32_t schema_version;
    char product[FIRMWARE_MANIFEST_PRODUCT_MAX_LEN];
    char channel[FIRMWARE_MANIFEST_CHANNEL_MAX_LEN];
    char version[FIRMWARE_MANIFEST_VERSION_MAX_LEN];
    char published_at[FIRMWARE_MANIFEST_TIMESTAMP_MAX_LEN];
    char git_sha[FIRMWARE_MANIFEST_GIT_SHA_LEN + 1];
    char idf_version[FIRMWARE_MANIFEST_IDF_VERSION_MAX_LEN];
    char device_model[FIRMWARE_MANIFEST_DEVICE_MODEL_MAX_LEN];
    char chip[FIRMWARE_MANIFEST_CHIP_MAX_LEN];
    char partition_layout[FIRMWARE_MANIFEST_PARTITION_LAYOUT_MAX_LEN];
    char firmware_url[FIRMWARE_MANIFEST_URL_MAX_LEN];
    uint32_t firmware_size;
    char firmware_sha256_hex[FIRMWARE_MANIFEST_SHA256_HEX_LEN + 1];
    uint8_t firmware_sha256[FIRMWARE_MANIFEST_SHA256_HEX_LEN / 2];
    uint32_t secure_version;
    bool mandatory;
    char release_url[FIRMWARE_MANIFEST_URL_MAX_LEN];
} firmware_manifest_t;

/**
 * @brief Parse and validate one complete OTA Manifest JSON document.
 *
 * json_len is the number of JSON bytes and does not include a trailing NUL.
 */
esp_err_t firmware_manifest_parse(const char *json,
                                  size_t json_len,
                                  firmware_manifest_t *manifest);

/**
 * @brief Validate runtime constraints that are not intrinsic to the document.
 */
esp_err_t firmware_manifest_validate_for_device(const firmware_manifest_t *manifest,
                                                const char *expected_channel,
                                                size_t ota_partition_size);

/**
 * @brief Compare the Manifest version with the running firmware version.
 *
 * result receives -1 when the Manifest is older, 0 when equal, and 1 when newer.
 */
esp_err_t firmware_manifest_compare_running_version(const firmware_manifest_t *manifest,
                                                    const char *running_version,
                                                    int *result);

#ifdef __cplusplus
}
#endif
