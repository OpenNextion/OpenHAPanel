#include "firmware_manifest.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "firmware_semver.h"

#ifndef OTA_RELEASE_REPOSITORY
#define OTA_RELEASE_REPOSITORY "OpenNextion/OpenHAPanel"
#endif

#define FIRMWARE_RELEASE_ASSET_PREFIX \
    FIRMWARE_MANIFEST_PRODUCT "-" CONFIG_DEVICE_MODEL_SLUG

#define FIRMWARE_RELEASE_DOWNLOAD_PREFIX \
    "https://github.com/" OTA_RELEASE_REPOSITORY "/releases/download/"
#define FIRMWARE_RELEASE_PAGE_PREFIX \
    "https://github.com/" OTA_RELEASE_REPOSITORY "/releases/tag/"

static const char *TAG = "firmware_manifest";

static const cJSON *manifest_get_unique_item(const cJSON *object, const char *name)
{
    if (!cJSON_IsObject(object) || !name) {
        return NULL;
    }

    const cJSON *match = NULL;
    for (const cJSON *item = object->child; item; item = item->next) {
        if (item->string && strcmp(item->string, name) == 0) {
            if (match) {
                return NULL;
            }
            match = item;
        }
    }
    return match;
}

static bool manifest_copy_string(const cJSON *object,
                                 const char *name,
                                 char *output,
                                 size_t output_size)
{
    const cJSON *item = manifest_get_unique_item(object, name);
    if (!cJSON_IsString(item) || !item->valuestring || item->valuestring[0] == '\0') {
        ESP_LOGE(TAG, "Missing or invalid string field: %s", name);
        return false;
    }

    size_t length = strlen(item->valuestring);
    if (length >= output_size) {
        ESP_LOGE(TAG, "String field is too long: %s", name);
        return false;
    }
    memcpy(output, item->valuestring, length + 1);
    return true;
}

static bool manifest_get_uint32(const cJSON *object, const char *name, uint32_t *output)
{
    const cJSON *item = manifest_get_unique_item(object, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 ||
        item->valuedouble > UINT32_MAX ||
        item->valuedouble != (double)(uint32_t)item->valuedouble) {
        ESP_LOGE(TAG, "Missing or invalid integer field: %s", name);
        return false;
    }
    *output = (uint32_t)item->valuedouble;
    return true;
}

static bool manifest_get_bool(const cJSON *object, const char *name, bool *output)
{
    const cJSON *item = manifest_get_unique_item(object, name);
    if (!cJSON_IsBool(item)) {
        ESP_LOGE(TAG, "Missing or invalid boolean field: %s", name);
        return false;
    }
    *output = cJSON_IsTrue(item);
    return true;
}

static bool manifest_is_hex(const char *value, size_t required_length)
{
    if (!value || strlen(value) != required_length) {
        return false;
    }
    for (size_t i = 0; i < required_length; i++) {
        if (!isxdigit((unsigned char)value[i])) {
            return false;
        }
    }
    return true;
}

static int manifest_hex_nibble(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return value - 'A' + 10;
}

static void manifest_decode_sha256(const char *hex, uint8_t *output)
{
    for (size_t i = 0; i < FIRMWARE_MANIFEST_SHA256_HEX_LEN / 2; i++) {
        output[i] = (uint8_t)((manifest_hex_nibble(hex[i * 2]) << 4) |
                              manifest_hex_nibble(hex[i * 2 + 1]));
    }
}

#if !OTA_ALLOW_INSECURE_HTTP
static bool manifest_has_prefix(const char *value, const char *prefix)
{
    return value && prefix && strncmp(value, prefix, strlen(prefix)) == 0;
}
#endif

static bool manifest_timestamp_is_valid(const char *timestamp)
{
    if (!timestamp || strlen(timestamp) != 20) {
        return false;
    }

    const size_t separators[] = {4, 7, 10, 13, 16, 19};
    const char expected[] = {'-', '-', 'T', ':', ':', 'Z'};
    for (size_t i = 0; i < sizeof(separators) / sizeof(separators[0]); i++) {
        if (timestamp[separators[i]] != expected[i]) {
            return false;
        }
    }
    for (size_t i = 0; i < 19; i++) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) {
            continue;
        }
        if (!isdigit((unsigned char)timestamp[i])) {
            return false;
        }
    }

    unsigned year = (unsigned)(timestamp[0] - '0') * 1000U +
                    (unsigned)(timestamp[1] - '0') * 100U +
                    (unsigned)(timestamp[2] - '0') * 10U +
                    (unsigned)(timestamp[3] - '0');
    unsigned month = (unsigned)(timestamp[5] - '0') * 10U +
                     (unsigned)(timestamp[6] - '0');
    unsigned day = (unsigned)(timestamp[8] - '0') * 10U +
                   (unsigned)(timestamp[9] - '0');
    unsigned hour = (unsigned)(timestamp[11] - '0') * 10U +
                    (unsigned)(timestamp[12] - '0');
    unsigned minute = (unsigned)(timestamp[14] - '0') * 10U +
                      (unsigned)(timestamp[15] - '0');
    unsigned second = (unsigned)(timestamp[17] - '0') * 10U +
                      (unsigned)(timestamp[18] - '0');

    if (year == 0 || month == 0 || month > 12 || hour > 23 || minute > 59 || second > 59) {
        return false;
    }

    static const uint8_t days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned max_day = days_per_month[month - 1];
    bool leap_year = (year % 4U == 0 && year % 100U != 0) || year % 400U == 0;
    if (month == 2 && leap_year) {
        max_day = 29;
    }
    return day > 0 && day <= max_day;
}

static bool manifest_document_consumed(const char *parse_end, const char *limit)
{
    while (parse_end < limit && isspace((unsigned char)*parse_end)) {
        parse_end++;
    }
    return parse_end == limit;
}

static bool manifest_urls_match_version(const firmware_manifest_t *manifest)
{
#if OTA_ALLOW_INSECURE_HTTP
    const char *separator = strstr(OTA_INFO_URL, "://");
    const char *last_slash = strrchr(OTA_INFO_URL, '/');
    if (!separator || !last_slash || last_slash <= separator + 2) {
        return false;
    }

    size_t base_length = (size_t)(last_slash - OTA_INFO_URL + 1);
    if (base_length >= FIRMWARE_MANIFEST_URL_MAX_LEN) {
        return false;
    }

    char base_url[FIRMWARE_MANIFEST_URL_MAX_LEN];
    memcpy(base_url, OTA_INFO_URL, base_length);
    base_url[base_length] = '\0';

    char expected_firmware_url[FIRMWARE_MANIFEST_URL_MAX_LEN];
    char expected_release_url[FIRMWARE_MANIFEST_URL_MAX_LEN];
    int firmware_length = snprintf(expected_firmware_url,
                                   sizeof(expected_firmware_url),
                                   "%s" FIRMWARE_RELEASE_ASSET_PREFIX ".ota.bin",
                                   base_url);
    int release_length = snprintf(expected_release_url,
                                  sizeof(expected_release_url),
                                  "%sv%s",
                                  base_url,
                                  manifest->version);
#else
    char expected_firmware_url[FIRMWARE_MANIFEST_URL_MAX_LEN];
    char expected_release_url[FIRMWARE_MANIFEST_URL_MAX_LEN];

    int firmware_length = snprintf(expected_firmware_url,
                                   sizeof(expected_firmware_url),
                                   FIRMWARE_RELEASE_DOWNLOAD_PREFIX
                                   "v%s/" FIRMWARE_RELEASE_ASSET_PREFIX ".ota.bin",
                                   manifest->version);
    int release_length = snprintf(expected_release_url,
                                  sizeof(expected_release_url),
                                  FIRMWARE_RELEASE_PAGE_PREFIX "v%s",
                                  manifest->version);
#endif
    if (firmware_length < 0 || (size_t)firmware_length >= sizeof(expected_firmware_url) ||
        release_length < 0 || (size_t)release_length >= sizeof(expected_release_url)) {
        return false;
    }

    return strcmp(manifest->firmware_url, expected_firmware_url) == 0 &&
           strcmp(manifest->release_url, expected_release_url) == 0;
}

esp_err_t firmware_manifest_parse(const char *json,
                                  size_t json_len,
                                  firmware_manifest_t *manifest)
{
    if (!json || !manifest || json_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (json_len > FIRMWARE_MANIFEST_MAX_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, json_len, &parse_end, false);
    if (!root || !cJSON_IsObject(root) || !parse_end ||
        !manifest_document_consumed(parse_end, json + json_len)) {
        ESP_LOGE(TAG, "Manifest is not one complete JSON object");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    firmware_manifest_t parsed = {0};
    const cJSON *build = manifest_get_unique_item(root, "build");
    const cJSON *target = manifest_get_unique_item(root, "target");
    const cJSON *firmware = manifest_get_unique_item(root, "firmware");

    bool valid = manifest_get_uint32(root, "schemaVersion", &parsed.schema_version) &&
                 manifest_copy_string(root, "product", parsed.product, sizeof(parsed.product)) &&
                 manifest_copy_string(root, "channel", parsed.channel, sizeof(parsed.channel)) &&
                 manifest_copy_string(root, "version", parsed.version, sizeof(parsed.version)) &&
                 manifest_copy_string(root, "publishedAt", parsed.published_at, sizeof(parsed.published_at)) &&
                 cJSON_IsObject(build) &&
                 manifest_copy_string(build, "gitSha", parsed.git_sha, sizeof(parsed.git_sha)) &&
                 manifest_copy_string(build, "idfVersion", parsed.idf_version, sizeof(parsed.idf_version)) &&
                 cJSON_IsObject(target) &&
                 manifest_copy_string(target,
                                      "model",
                                      parsed.device_model,
                                      sizeof(parsed.device_model)) &&
                 manifest_copy_string(target, "chip", parsed.chip, sizeof(parsed.chip)) &&
                 manifest_copy_string(target,
                                      "partitionLayout",
                                      parsed.partition_layout,
                                      sizeof(parsed.partition_layout)) &&
                 cJSON_IsObject(firmware) &&
                 manifest_copy_string(firmware, "url", parsed.firmware_url, sizeof(parsed.firmware_url)) &&
                 manifest_get_uint32(firmware, "size", &parsed.firmware_size) &&
                 manifest_copy_string(firmware,
                                      "sha256",
                                      parsed.firmware_sha256_hex,
                                      sizeof(parsed.firmware_sha256_hex)) &&
                 manifest_get_uint32(root, "secureVersion", &parsed.secure_version) &&
                 manifest_get_bool(root, "mandatory", &parsed.mandatory) &&
                 manifest_copy_string(root, "releaseUrl", parsed.release_url, sizeof(parsed.release_url));

    if (!valid) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    bool urls_valid = manifest_urls_match_version(&parsed);
#if !OTA_ALLOW_INSECURE_HTTP
    urls_valid = urls_valid &&
                 manifest_has_prefix(parsed.firmware_url, FIRMWARE_RELEASE_DOWNLOAD_PREFIX) &&
                 manifest_has_prefix(parsed.release_url, FIRMWARE_RELEASE_PAGE_PREFIX);
#endif

    valid = parsed.schema_version == FIRMWARE_MANIFEST_SCHEMA_VERSION &&
            strcmp(parsed.product, FIRMWARE_MANIFEST_PRODUCT) == 0 &&
            (strcmp(parsed.channel, "stable") == 0 || strcmp(parsed.channel, "beta") == 0) &&
            firmware_semver_is_valid(parsed.version) &&
            manifest_timestamp_is_valid(parsed.published_at) &&
            manifest_is_hex(parsed.git_sha, FIRMWARE_MANIFEST_GIT_SHA_LEN) &&
            strcmp(parsed.chip, FIRMWARE_MANIFEST_CHIP) == 0 &&
            strcmp(parsed.partition_layout, FIRMWARE_MANIFEST_PARTITION_LAYOUT) == 0 &&
            parsed.firmware_size > 0 &&
            manifest_is_hex(parsed.firmware_sha256_hex, FIRMWARE_MANIFEST_SHA256_HEX_LEN) &&
            urls_valid;

    if (!valid) {
        ESP_LOGE(TAG, "Manifest identity or value validation failed");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    manifest_decode_sha256(parsed.firmware_sha256_hex, parsed.firmware_sha256);
    *manifest = parsed;
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t firmware_manifest_validate_for_device(const firmware_manifest_t *manifest,
                                                const char *expected_channel,
                                                size_t ota_partition_size)
{
    if (!manifest || !expected_channel || ota_partition_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(expected_channel, "stable") != 0 && strcmp(expected_channel, "beta") != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(manifest->channel, expected_channel) != 0) {
        ESP_LOGE(TAG,
                 "Manifest channel mismatch: expected=%s actual=%s",
                 expected_channel,
                 manifest->channel);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (strcmp(manifest->device_model, CONFIG_DEVICE_MODEL_STRING) != 0) {
        ESP_LOGE(TAG,
                 "Manifest device model mismatch: expected=%s actual=%s",
                 CONFIG_DEVICE_MODEL_STRING,
                 manifest->device_model);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (manifest->firmware_size > ota_partition_size) {
        ESP_LOGE(TAG,
                 "Firmware is too large: image=%lu partition=%lu",
                 (unsigned long)manifest->firmware_size,
                 (unsigned long)ota_partition_size);
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t firmware_manifest_compare_running_version(const firmware_manifest_t *manifest,
                                                    const char *running_version,
                                                    int *result)
{
    if (!manifest || !running_version || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    return firmware_semver_compare(manifest->version, running_version, result);
}
