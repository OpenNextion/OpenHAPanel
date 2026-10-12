#include "firmware_download.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"

#define FIRMWARE_DOWNLOAD_HTTP_TIMEOUT_MS 30000
#define FIRMWARE_DOWNLOAD_HTTP_BUFFER_SIZE 8192
#define FIRMWARE_DOWNLOAD_HTTP_TX_BUFFER_SIZE 2048
#define FIRMWARE_DOWNLOAD_HASH_BUFFER_SIZE 4096
#define FIRMWARE_DOWNLOAD_HASH_YIELD_INTERVAL (64U * 1024U)
#define FIRMWARE_DOWNLOAD_ERASE_BLOCK_SIZE (64U * 1024U)
#define FIRMWARE_DOWNLOAD_ERASE_YIELD_MS 10U
#define FIRMWARE_DOWNLOAD_MAX_REDIRECTS 5

static const char *TAG = "firmware_download";

static esp_err_t firmware_download_begin_pre_erased_ota(
    const esp_partition_t *partition,
    size_t image_size,
    esp_ota_handle_t *ota_handle);

typedef struct {
    esp_ota_handle_t ota_handle;
    size_t expected_size;
    size_t bytes_written;
    uint8_t last_progress;
    esp_err_t error;
    bool ignore_body;
    firmware_download_progress_cb_t progress_cb;
    void *user_context;
} firmware_download_http_context_t;

static bool firmware_download_url_is_https(const char *url)
{
    return url && strncmp(url, "https://", strlen("https://")) == 0;
}

static bool firmware_download_url_is_allowed(const char *url)
{
    if (firmware_download_url_is_https(url)) {
        return true;
    }
#if OTA_ALLOW_INSECURE_HTTP
    return url && strncmp(url, "http://", strlen("http://")) == 0;
#else
    return false;
#endif
}

static bool firmware_download_string_equals(const char *value,
                                            size_t value_capacity,
                                            const char *expected)
{
    size_t expected_length = strlen(expected);
    return expected_length < value_capacity &&
           strnlen(value, value_capacity) == expected_length &&
           memcmp(value, expected, expected_length) == 0;
}

static bool firmware_download_hash_equals(const uint8_t *left,
                                          const uint8_t *right,
                                          size_t length)
{
    uint8_t difference = 0;
    for (size_t index = 0; index < length; index++) {
        difference |= left[index] ^ right[index];
    }
    return difference == 0;
}

static esp_err_t firmware_download_begin_pre_erased_ota(
    const esp_partition_t *partition,
    size_t image_size,
    esp_ota_handle_t *ota_handle)
{
    if (!partition || !ota_handle || image_size == 0 ||
        partition->erase_size == 0 || image_size > partition->size) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Let the OTA subsystem validate the partition and invalidate any stale
     * boot selection metadata, but do not let it erase the image one sector
     * at a time while HTTP data is arriving. */
    esp_ota_handle_t preparation_handle = 0;
    esp_err_t err = esp_ota_begin(partition,
                                  OTA_WITH_SEQUENTIAL_WRITES,
                                  &preparation_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_ota_abort(preparation_handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t erase_size = ((image_size + partition->erase_size - 1U) /
                         partition->erase_size) * partition->erase_size;
    if (erase_size > partition->size) {
        return ESP_ERR_INVALID_SIZE;
    }

    int64_t erase_started_us = esp_timer_get_time();
    int64_t max_block_elapsed_us = 0;
    size_t erased = 0;
    while (erased < erase_size) {
        size_t remaining = erase_size - erased;
        size_t chunk_size = remaining < FIRMWARE_DOWNLOAD_ERASE_BLOCK_SIZE
                                ? remaining
                                : FIRMWARE_DOWNLOAD_ERASE_BLOCK_SIZE;
        int64_t block_started_us = esp_timer_get_time();
        err = esp_partition_erase_range(partition, erased, chunk_size);
        int64_t block_elapsed_us = esp_timer_get_time() - block_started_us;
        if (block_elapsed_us > max_block_elapsed_us) {
            max_block_elapsed_us = block_elapsed_us;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "OTA partition erase failed: offset=%lu size=%lu error=%s",
                     (unsigned long)erased,
                     (unsigned long)chunk_size,
                     esp_err_to_name(err));
            return err;
        }
        erased += chunk_size;

        /* One flash IPC operation is limited to at most one 64 KiB block.
         * Keep the staging task blocked for a real scheduling window after
         * every block. A one-tick delay was too short under normal UI/network
         * load and could starve IDLE0 for the complete multi-second erase,
         * triggering the task watchdog. */
        vTaskDelay(pdMS_TO_TICKS(FIRMWARE_DOWNLOAD_ERASE_YIELD_MS));
    }

    int64_t erase_elapsed_ms = (esp_timer_get_time() - erase_started_us) / 1000;
    ESP_LOGI(TAG,
             "OTA partition pre-erased: bytes=%lu elapsed=%lld ms blocks=%lu max_block=%lld ms yield=%u ms",
             (unsigned long)erase_size,
             (long long)erase_elapsed_ms,
             (unsigned long)((erase_size + FIRMWARE_DOWNLOAD_ERASE_BLOCK_SIZE - 1U) /
                             FIRMWARE_DOWNLOAD_ERASE_BLOCK_SIZE),
             (long long)(max_block_elapsed_us / 1000),
             (unsigned)FIRMWARE_DOWNLOAD_ERASE_YIELD_MS);

    /* The required range is already erased. A bulk-style resume handle keeps
     * esp_ota_write() from erasing every incoming HTTP chunk again. */
    return esp_ota_resume(partition, erase_size, 0, ota_handle);
}

static esp_err_t firmware_download_partition_sha256(const esp_partition_t *partition,
                                                    size_t image_size,
                                                    uint8_t output[32])
{
    uint8_t *buffer = malloc(FIRMWARE_DOWNLOAD_HASH_BUFFER_SIZE);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }

    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    int result = mbedtls_sha256_starts(&context, 0);
    size_t offset = 0;

    while (result == 0 && offset < image_size) {
        size_t remaining = image_size - offset;
        size_t chunk_size = remaining < FIRMWARE_DOWNLOAD_HASH_BUFFER_SIZE
                                ? remaining
                                : FIRMWARE_DOWNLOAD_HASH_BUFFER_SIZE;
        esp_err_t err = esp_partition_read(partition, offset, buffer, chunk_size);
        if (err != ESP_OK) {
            mbedtls_sha256_free(&context);
            free(buffer);
            return err;
        }
        result = mbedtls_sha256_update(&context, buffer, chunk_size);
        offset += chunk_size;
        if (result == 0 && offset < image_size &&
            offset % FIRMWARE_DOWNLOAD_HASH_YIELD_INTERVAL == 0) {
            vTaskDelay(1);
        }
    }

    if (result == 0) {
        result = mbedtls_sha256_finish(&context, output);
    }
    mbedtls_sha256_free(&context);
    free(buffer);
    return result == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t firmware_download_validate_description(const firmware_manifest_t *manifest,
                                                        const esp_app_desc_t *description)
{
    if (!firmware_download_string_equals(description->project_name,
                                         sizeof(description->project_name),
                                         FIRMWARE_MANIFEST_PRODUCT)) {
        ESP_LOGE(TAG, "Image project does not match Manifest: %.32s", description->project_name);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (!firmware_download_string_equals(description->version,
                                         sizeof(description->version),
                                         manifest->version)) {
        ESP_LOGE(TAG,
                 "Image version does not match Manifest: image=%.32s manifest=%s",
                 description->version,
                 manifest->version);
        return ESP_ERR_INVALID_VERSION;
    }
    if (description->secure_version != manifest->secure_version) {
        ESP_LOGE(TAG,
                 "Image secure version does not match Manifest: image=%lu manifest=%lu",
                 (unsigned long)description->secure_version,
                 (unsigned long)manifest->secure_version);
        return ESP_ERR_INVALID_VERSION;
    }
    return ESP_OK;
}

static void firmware_download_report(firmware_download_progress_cb_t progress_cb,
                                     void *user_context,
                                     firmware_download_stage_t stage,
                                     uint8_t progress)
{
    if (progress_cb) {
        progress_cb(stage, progress, user_context);
    }
}

static esp_err_t firmware_download_http_event(esp_http_client_event_t *event)
{
    firmware_download_http_context_t *context = event ? event->user_data : NULL;
    if (!context) {
        return ESP_ERR_INVALID_ARG;
    }
    if (context->error != ESP_OK) {
        return context->error;
    }

    if (event->event_id == HTTP_EVENT_HEADERS_SENT) {
        context->ignore_body = false;
        return ESP_OK;
    }
    if (event->event_id == HTTP_EVENT_REDIRECT) {
        context->ignore_body = true;
        return ESP_OK;
    }
    if (event->event_id != HTTP_EVENT_ON_DATA || context->ignore_body ||
        !event->data || event->data_len <= 0) {
        return ESP_OK;
    }

    size_t data_length = (size_t)event->data_len;
    if (data_length > context->expected_size - context->bytes_written) {
        context->error = ESP_ERR_INVALID_SIZE;
        return context->error;
    }

    context->error = esp_ota_write(context->ota_handle,
                                   event->data,
                                   data_length);
    if (context->error != ESP_OK) {
        return context->error;
    }

    context->bytes_written += data_length;
    uint32_t percentage = ((uint64_t)context->bytes_written * 100U) /
                          context->expected_size;
    uint8_t progress = percentage > 99U ? 99U : (uint8_t)percentage;
    if (progress != context->last_progress) {
        firmware_download_report(context->progress_cb,
                                 context->user_context,
                                 FIRMWARE_DOWNLOAD_STAGE_DOWNLOADING,
                                 progress);
        context->last_progress = progress;
    }
    return ESP_OK;
}

esp_err_t firmware_download_stage(const firmware_manifest_t *manifest,
                                  const esp_partition_t *update_partition,
                                  firmware_download_progress_cb_t progress_cb,
                                  void *user_context)
{
    if (!manifest || !update_partition ||
        !firmware_download_url_is_allowed(manifest->firmware_url) ||
        manifest->firmware_size == 0 ||
        manifest->firmware_size > update_partition->size) {
        return ESP_ERR_INVALID_ARG;
    }

    bool use_tls = firmware_download_url_is_https(manifest->firmware_url);
    firmware_download_http_context_t download_context = {
        .ota_handle = 0,
        .expected_size = manifest->firmware_size,
        .bytes_written = 0,
        .last_progress = UINT8_MAX,
        .error = ESP_OK,
        .progress_cb = progress_cb,
        .user_context = user_context,
    };
    esp_http_client_config_t http_config = {
        .url = manifest->firmware_url,
        .timeout_ms = FIRMWARE_DOWNLOAD_HTTP_TIMEOUT_MS,
        .disable_auto_redirect = false,
        .max_redirection_count = FIRMWARE_DOWNLOAD_MAX_REDIRECTS,
        .buffer_size = FIRMWARE_DOWNLOAD_HTTP_BUFFER_SIZE,
        .buffer_size_tx = FIRMWARE_DOWNLOAD_HTTP_TX_BUFFER_SIZE,
        .crt_bundle_attach = use_tls ? esp_crt_bundle_attach : NULL,
        .keep_alive_enable = true,
        .event_handler = firmware_download_http_event,
        .user_data = &download_context,
    };

    if (!use_tls) {
        ESP_LOGW(TAG, "Downloading firmware over insecure HTTP test transport");
    }
    ESP_LOGI(TAG,
             "Starting firmware download: version=%s size=%lu partition=%s url=%s",
             manifest->version,
             (unsigned long)manifest->firmware_size,
             update_partition->label,
             manifest->firmware_url);

    int64_t total_started_us = esp_timer_get_time();
    esp_err_t err = firmware_download_begin_pre_erased_ota(
        update_partition,
        manifest->firmware_size,
        &download_context.ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Preparing OTA partition failed: %s", esp_err_to_name(err));
        return err;
    }

    if (use_tls) {
        ESP_LOGI(TAG,
                 "TLS heap before firmware request: internal_free=%u internal_largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        err = ESP_ERR_NO_MEM;
        goto abort;
    }
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    int64_t transfer_started_us = esp_timer_get_time();
    err = esp_http_client_perform(client);
    int64_t transfer_elapsed_ms =
        (esp_timer_get_time() - transfer_started_us) / 1000;
    if (err == ESP_OK && download_context.error != ESP_OK) {
        err = download_context.error;
    }
    int status_code = esp_http_client_get_status_code(client);
    int64_t content_length = esp_http_client_get_content_length(client);
    esp_http_client_cleanup(client);
    client = NULL;
    uint64_t transfer_rate_kib = transfer_elapsed_ms > 0
                                     ? ((uint64_t)download_context.bytes_written * 1000U) /
                                           (uint64_t)transfer_elapsed_ms / 1024U
                                     : 0;
    ESP_LOGI(TAG,
             "Firmware transfer finished: bytes=%lu elapsed=%lld ms average=%llu KiB/s",
             (unsigned long)download_context.bytes_written,
             (long long)transfer_elapsed_ms,
             (unsigned long long)transfer_rate_kib);
    if (err != ESP_OK || status_code < 200 || status_code >= 300) {
        if (err == ESP_OK) {
            err = ESP_ERR_INVALID_RESPONSE;
        }
        ESP_LOGE(TAG, "Firmware download failed: %s", esp_err_to_name(err));
        goto abort;
    }
    if (download_context.bytes_written != manifest->firmware_size ||
        (content_length >= 0 && (uint64_t)content_length != manifest->firmware_size)) {
        ESP_LOGE(TAG,
                 "Firmware size mismatch: read=%lu content_length=%lld expected=%lu",
                 (unsigned long)download_context.bytes_written,
                 (long long)content_length,
                 (unsigned long)manifest->firmware_size);
        err = ESP_ERR_INVALID_SIZE;
        goto abort;
    }

    int64_t ota_end_started_us = esp_timer_get_time();
    err = esp_ota_end(download_context.ota_handle);
    download_context.ota_handle = 0;
    ESP_LOGI(TAG,
             "OTA image finalization finished: elapsed=%lld ms",
             (long long)((esp_timer_get_time() - ota_end_started_us) / 1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Downloaded OTA image is invalid: %s", esp_err_to_name(err));
        return err;
    }

    firmware_download_report(progress_cb,
                             user_context,
                             FIRMWARE_DOWNLOAD_STAGE_VERIFYING,
                             100);

    int64_t verification_started_us = esp_timer_get_time();
    esp_app_desc_t description = {0};
    err = esp_ota_get_partition_description(update_partition, &description);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read staged image description: %s", esp_err_to_name(err));
        return err;
    }
    err = firmware_download_validate_description(manifest, &description);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t image_sha256[32];
    err = firmware_download_partition_sha256(update_partition,
                                             manifest->firmware_size,
                                             image_sha256);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot calculate downloaded image SHA-256: %s", esp_err_to_name(err));
        return err;
    }
    if (!firmware_download_hash_equals(image_sha256,
                                       manifest->firmware_sha256,
                                       sizeof(image_sha256))) {
        ESP_LOGE(TAG, "Downloaded image SHA-256 does not match Manifest");
        err = ESP_ERR_INVALID_CRC;
        return err;
    }

    ESP_LOGI(TAG,
             "Firmware verification finished: elapsed=%lld ms total=%lld ms",
             (long long)((esp_timer_get_time() - verification_started_us) / 1000),
             (long long)((esp_timer_get_time() - total_started_us) / 1000));

    ESP_LOGI(TAG,
             "Firmware staged without changing boot partition: version=%s bytes=%lu partition=%s",
             manifest->version,
             (unsigned long)manifest->firmware_size,
             update_partition->label);
    return ESP_OK;

abort:
    if (client) {
        esp_http_client_cleanup(client);
    }
    if (download_context.ota_handle) {
        esp_err_t abort_err = esp_ota_abort(download_context.ota_handle);
        if (abort_err != ESP_OK) {
            ESP_LOGW(TAG, "esp_ota_abort failed: %s", esp_err_to_name(abort_err));
        }
    }
    return err;
}

esp_err_t firmware_download_validate_staged(
    const firmware_update_staged_info_t *staged,
    const esp_partition_t *partition)
{
    if (!staged || !staged->valid || !partition || staged->image_size == 0 ||
        staged->image_size > partition->size ||
        strcmp(staged->partition_label, partition->label) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_app_desc_t description = {0};
    esp_err_t err = esp_ota_get_partition_description(partition, &description);
    if (err != ESP_OK) {
        return err;
    }
    if (!firmware_download_string_equals(description.project_name,
                                         sizeof(description.project_name),
                                         FIRMWARE_MANIFEST_PRODUCT) ||
        !firmware_download_string_equals(description.version,
                                         sizeof(description.version),
                                         staged->version)) {
        return ESP_ERR_INVALID_VERSION;
    }

    uint8_t actual_sha256[32];
    err = firmware_download_partition_sha256(partition,
                                             staged->image_size,
                                             actual_sha256);
    if (err != ESP_OK) {
        return err;
    }
    return firmware_download_hash_equals(actual_sha256,
                                         staged->image_sha256,
                                         sizeof(actual_sha256))
               ? ESP_OK
               : ESP_ERR_INVALID_CRC;
}
