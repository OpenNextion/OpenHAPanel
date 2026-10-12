#include "firmware_update.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "firmware_boot_check.h"
#include "firmware_download.h"
#include "firmware_manifest_download.h"
#include "firmware_storage.h"
#include "firmware_update_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "network.h"
#include "sdkconfig.h"

#define FIRMWARE_UPDATE_CHECK_TASK_STACK_SIZE 8192
#define FIRMWARE_UPDATE_INSTALL_TASK_STACK_SIZE 10240
#define FIRMWARE_UPDATE_ACTIVATE_TASK_STACK_SIZE 4096
#define FIRMWARE_UPDATE_REBOOT_TASK_STACK_SIZE 2048
#define FIRMWARE_UPDATE_CHECK_TASK_PRIORITY 4
#define FIRMWARE_UPDATE_INSTALL_TASK_PRIORITY 5
#define FIRMWARE_UPDATE_ACTIVATE_TASK_PRIORITY 5
#define FIRMWARE_UPDATE_REBOOT_TASK_PRIORITY 5
#define FIRMWARE_UPDATE_REBOOT_DELAY_MS 1000
#define FIRMWARE_UPDATE_VALID_TIME_MINIMUM 1577836800

ESP_EVENT_DEFINE_BASE(FIRMWARE_UPDATE_EVENT);

static const char *TAG = "firmware_update";

static SemaphoreHandle_t s_status_lock;
static firmware_update_status_t s_status;
static firmware_manifest_t s_available_manifest;
static bool s_available_manifest_valid;
static bool s_factory_resetting;
static portMUX_TYPE s_operation_guard = portMUX_INITIALIZER_UNLOCKED;
static bool s_operation_claimed;

typedef struct {
    firmware_manifest_t manifest;
    bool automatic;
} firmware_update_install_args_t;

typedef struct {
    bool preserve_staged;
    firmware_update_staged_info_t staged;
} firmware_update_check_args_t;

static void firmware_update_check_task(void *argument);
static void firmware_update_install_task(void *argument);
static void firmware_update_activate_task(void *argument);
static void firmware_update_reboot_task(void *argument);
static void firmware_update_install_progress(firmware_download_stage_t stage,
                                             uint8_t progress,
                                             void *user_context);
static esp_err_t firmware_update_install_async_internal(bool automatic);
static bool firmware_update_claim_operation(void);
static void firmware_update_release_operation(void);
static void firmware_update_clear_available_manifest(void);
static void firmware_update_store_available_manifest(const firmware_manifest_t *manifest);
static esp_err_t firmware_update_check_prerequisites(const char *operation,
                                                     const char *request_url,
                                                     const esp_partition_t **update_partition);
static const esp_partition_t *firmware_update_find_staged_partition(
    const firmware_update_staged_info_t *staged);

static bool firmware_update_transition_allowed(firmware_update_state_t from,
                                               firmware_update_state_t to)
{
    if (from == to) {
        return true;
    }

    switch (from) {
    case FIRMWARE_UPDATE_STATE_IDLE:
        return to == FIRMWARE_UPDATE_STATE_CHECKING ||
               to == FIRMWARE_UPDATE_STATE_STAGED ||
               to == FIRMWARE_UPDATE_STATE_BOOT_VERIFYING ||
               to == FIRMWARE_UPDATE_STATE_SUCCESS ||
               to == FIRMWARE_UPDATE_STATE_ROLLED_BACK;

    case FIRMWARE_UPDATE_STATE_CHECKING:
        return to == FIRMWARE_UPDATE_STATE_UP_TO_DATE ||
               to == FIRMWARE_UPDATE_STATE_AVAILABLE ||
               to == FIRMWARE_UPDATE_STATE_STAGED ||
               to == FIRMWARE_UPDATE_STATE_FAILED;

    case FIRMWARE_UPDATE_STATE_UP_TO_DATE:
        return to == FIRMWARE_UPDATE_STATE_IDLE ||
               to == FIRMWARE_UPDATE_STATE_CHECKING;

    case FIRMWARE_UPDATE_STATE_AVAILABLE:
        return to == FIRMWARE_UPDATE_STATE_IDLE ||
               to == FIRMWARE_UPDATE_STATE_CHECKING ||
               to == FIRMWARE_UPDATE_STATE_DOWNLOADING;

    case FIRMWARE_UPDATE_STATE_DOWNLOADING:
        return to == FIRMWARE_UPDATE_STATE_VERIFYING ||
               to == FIRMWARE_UPDATE_STATE_FAILED;

    case FIRMWARE_UPDATE_STATE_VERIFYING:
        return to == FIRMWARE_UPDATE_STATE_STAGED ||
               to == FIRMWARE_UPDATE_STATE_FAILED;

    case FIRMWARE_UPDATE_STATE_STAGED:
        return to == FIRMWARE_UPDATE_STATE_ACTIVATING ||
               to == FIRMWARE_UPDATE_STATE_CHECKING ||
               to == FIRMWARE_UPDATE_STATE_IDLE;

    case FIRMWARE_UPDATE_STATE_ACTIVATING:
        return to == FIRMWARE_UPDATE_STATE_READY_TO_REBOOT ||
               to == FIRMWARE_UPDATE_STATE_STAGED ||
               to == FIRMWARE_UPDATE_STATE_FAILED;

    case FIRMWARE_UPDATE_STATE_READY_TO_REBOOT:
        return to == FIRMWARE_UPDATE_STATE_REBOOTING ||
               to == FIRMWARE_UPDATE_STATE_FAILED;

    case FIRMWARE_UPDATE_STATE_REBOOTING:
        return to == FIRMWARE_UPDATE_STATE_FAILED;

    case FIRMWARE_UPDATE_STATE_BOOT_VERIFYING:
        return to == FIRMWARE_UPDATE_STATE_SUCCESS ||
               to == FIRMWARE_UPDATE_STATE_ROLLED_BACK ||
               to == FIRMWARE_UPDATE_STATE_FAILED;

    case FIRMWARE_UPDATE_STATE_SUCCESS:
    case FIRMWARE_UPDATE_STATE_ROLLED_BACK:
    case FIRMWARE_UPDATE_STATE_FAILED:
        return to == FIRMWARE_UPDATE_STATE_IDLE ||
               to == FIRMWARE_UPDATE_STATE_CHECKING;

    default:
        return false;
    }
}

static bool firmware_update_state_is_busy(firmware_update_state_t state)
{
    switch (state) {
    case FIRMWARE_UPDATE_STATE_CHECKING:
    case FIRMWARE_UPDATE_STATE_DOWNLOADING:
    case FIRMWARE_UPDATE_STATE_VERIFYING:
    case FIRMWARE_UPDATE_STATE_ACTIVATING:
    case FIRMWARE_UPDATE_STATE_READY_TO_REBOOT:
    case FIRMWARE_UPDATE_STATE_REBOOTING:
    case FIRMWARE_UPDATE_STATE_BOOT_VERIFYING:
        return true;
    default:
        return false;
    }
}

esp_err_t firmware_update_init(void)
{
    if (s_status_lock) {
        return ESP_OK;
    }

    s_status_lock = xSemaphoreCreateMutex();
    if (!s_status_lock) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_factory_resetting = false;
    s_status.state = FIRMWARE_UPDATE_STATE_IDLE;
    s_status.last_error = ESP_OK;
    s_status.revision = 1;
    strlcpy(s_status.current_version,
            CONFIG_FIRMWARE_VERSION_STRING,
            sizeof(s_status.current_version));

    esp_err_t storage_err = firmware_storage_init(CONFIG_FIRMWARE_VERSION_STRING);
    if (storage_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OTA history: %s", esp_err_to_name(storage_err));
    }

    firmware_boot_check_result_t boot_result = {0};
    esp_err_t boot_err = firmware_boot_check_inspect(&boot_result);
    if (boot_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to inspect OTA boot state: %s", esp_err_to_name(boot_err));
    }

    ESP_LOGI(TAG,
             "Initialized: current_version=%s default_channel=%s auto_check=%d "
             "auto_check_on_wifi_connect=%d",
             s_status.current_version,
             OTA_DEFAULT_CHANNEL_STRING,
             OTA_AUTO_CHECK_DEFAULT,
             OTA_AUTO_CHECK_ON_WIFI_CONNECT);
#if OTA_ALLOW_INSECURE_HTTP
    ESP_LOGW(TAG, "INSECURE LOCAL OTA TEST MODE IS ENABLED; DO NOT RELEASE THIS BUILD");
#endif

    if (boot_err == ESP_OK && boot_result.state == FIRMWARE_BOOT_CHECK_PENDING) {
        firmware_update_transition(FIRMWARE_UPDATE_STATE_BOOT_VERIFYING,
                                   0,
                                   ESP_OK,
                                   boot_result.target_version);
    }
    else if (boot_err == ESP_OK &&
             boot_result.state == FIRMWARE_BOOT_CHECK_ROLLED_BACK) {
        firmware_update_transition(FIRMWARE_UPDATE_STATE_ROLLED_BACK,
                                   0,
                                   boot_result.last_error,
                                   boot_result.target_version);
    }
    else if (boot_err == ESP_OK &&
             boot_result.state == FIRMWARE_BOOT_CHECK_NORMAL) {
        firmware_update_staged_info_t staged = {0};
        esp_err_t staged_err = firmware_storage_get_staged(&staged);
        const esp_partition_t *staged_partition =
            staged_err == ESP_OK ? firmware_update_find_staged_partition(&staged) : NULL;
        if (staged_err == ESP_OK && staged_partition) {
            ESP_LOGI(TAG,
                     "Recovered staged firmware: version=%s partition=%s size=%lu",
                     staged.version,
                     staged.partition_label,
                     (unsigned long)staged.image_size);
            firmware_update_transition(FIRMWARE_UPDATE_STATE_STAGED,
                                       100,
                                       ESP_OK,
                                       staged.version);
        }
        else if (staged_err != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG,
                     "Discarding invalid staged firmware metadata: %s",
                     staged_err == ESP_OK ? "partition not found" : esp_err_to_name(staged_err));
            firmware_storage_clear_staged();
        }
    }

    return storage_err != ESP_OK ? storage_err : boot_err;
}

esp_err_t firmware_update_check_async(void)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!firmware_update_claim_operation()) {
        return ESP_ERR_INVALID_STATE;
    }

    firmware_update_check_args_t *args = calloc(1, sizeof(*args));
    if (!args) {
        firmware_update_release_operation();
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    args->preserve_staged = s_status.state == FIRMWARE_UPDATE_STATE_STAGED;
    xSemaphoreGive(s_status_lock);
    if (args->preserve_staged) {
        esp_err_t staged_err = firmware_storage_get_staged(&args->staged);
        if (staged_err != ESP_OK ||
            !firmware_update_find_staged_partition(&args->staged)) {
            free(args);
            firmware_update_release_operation();
            return staged_err == ESP_OK ? ESP_ERR_NOT_FOUND : staged_err;
        }
    }

    esp_err_t err = firmware_update_transition(FIRMWARE_UPDATE_STATE_CHECKING,
                                                0,
                                                ESP_OK,
                                                NULL);
    if (err != ESP_OK) {
        free(args);
        firmware_update_release_operation();
        return err;
    }
    firmware_update_clear_available_manifest();

    BaseType_t created = xTaskCreateWithCaps(firmware_update_check_task,
                                             "ota_manifest_check",
                                             FIRMWARE_UPDATE_CHECK_TASK_STACK_SIZE,
                                             args,
                                             FIRMWARE_UPDATE_CHECK_TASK_PRIORITY,
                                             NULL,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        err = ESP_ERR_NO_MEM;
        if (args->preserve_staged) {
            firmware_update_transition(FIRMWARE_UPDATE_STATE_STAGED,
                                       100,
                                       err,
                                       args->staged.version);
        }
        else {
            firmware_update_transition(FIRMWARE_UPDATE_STATE_FAILED, 0, err, NULL);
        }
        free(args);
        firmware_update_release_operation();
        return err;
    }

    ESP_LOGI(TAG,
             "Manifest check task created with %u-byte PSRAM stack: internal_free=%u "
             "internal_largest=%u",
             (unsigned)FIRMWARE_UPDATE_CHECK_TASK_STACK_SIZE,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    return ESP_OK;
}

esp_err_t firmware_update_install_async(void)
{
    return firmware_update_install_async_internal(false);
}

esp_err_t firmware_update_install_automatic_async(void)
{
    return firmware_update_install_async_internal(true);
}

static esp_err_t firmware_update_install_async_internal(bool automatic)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!firmware_update_claim_operation()) {
        return ESP_ERR_INVALID_STATE;
    }

    firmware_update_install_args_t *args = malloc(sizeof(*args));
    if (!args) {
        firmware_update_release_operation();
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    bool available = s_available_manifest_valid &&
                     s_status.state == FIRMWARE_UPDATE_STATE_AVAILABLE;
    if (available) {
        args->manifest = s_available_manifest;
        args->automatic = automatic;
    }
    xSemaphoreGive(s_status_lock);

    if (!available) {
        free(args);
        firmware_update_release_operation();
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = firmware_update_transition(FIRMWARE_UPDATE_STATE_DOWNLOADING,
                                                0,
                                                ESP_OK,
                                                args->manifest.version);
    if (err != ESP_OK) {
        free(args);
        firmware_update_release_operation();
        return err;
    }

    /*
     * Keep this task's stack in internal RAM. Staging erases and writes the
     * OTA partition, which temporarily disables the external-memory cache.
     * A PSRAM-backed stack is inaccessible while the cache is frozen and
     * causes s_task_stack_is_sane_when_cache_frozen() to assert.
     */
    BaseType_t created = xTaskCreate(firmware_update_install_task,
                                     "ota_stage",
                                     FIRMWARE_UPDATE_INSTALL_TASK_STACK_SIZE,
                                     args,
                                     FIRMWARE_UPDATE_INSTALL_TASK_PRIORITY,
                                     NULL);
    if (created != pdPASS) {
        err = ESP_ERR_NO_MEM;
        free(args);
        firmware_update_transition(FIRMWARE_UPDATE_STATE_FAILED,
                                   0,
                                   err,
                                   NULL);
        firmware_update_release_operation();
        return err;
    }

    return ESP_OK;
}

esp_err_t firmware_update_activate_staged(void)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!firmware_update_claim_operation()) {
        return ESP_ERR_INVALID_STATE;
    }

    firmware_update_staged_info_t *staged = malloc(sizeof(*staged));
    if (!staged) {
        firmware_update_release_operation();
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = firmware_storage_get_staged(staged);
    if (err != ESP_OK) {
        free(staged);
        firmware_update_release_operation();
        return err;
    }

    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    bool can_activate = s_status.state == FIRMWARE_UPDATE_STATE_STAGED;
    xSemaphoreGive(s_status_lock);
    if (!can_activate) {
        free(staged);
        firmware_update_release_operation();
        return ESP_ERR_INVALID_STATE;
    }

    err = firmware_update_transition(FIRMWARE_UPDATE_STATE_ACTIVATING,
                                     100,
                                     ESP_OK,
                                     staged->version);
    if (err != ESP_OK) {
        free(staged);
        firmware_update_release_operation();
        return err;
    }

    BaseType_t created = xTaskCreate(firmware_update_activate_task,
                                     "ota_activate",
                                     FIRMWARE_UPDATE_ACTIVATE_TASK_STACK_SIZE,
                                     staged,
                                     FIRMWARE_UPDATE_ACTIVATE_TASK_PRIORITY,
                                     NULL);
    if (created != pdPASS) {
        free(staged);
        err = ESP_ERR_NO_MEM;
        firmware_update_transition(FIRMWARE_UPDATE_STATE_STAGED, 100, err, NULL);
        firmware_update_release_operation();
        return err;
    }
    return ESP_OK;
}

esp_err_t firmware_update_get_staged_info(firmware_update_staged_info_t *info)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    return firmware_storage_get_staged(info);
}

esp_err_t firmware_update_confirm_boot(void)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    bool image_confirmed = false;
    esp_err_t err = firmware_boot_check_confirm(&image_confirmed);
    if (image_confirmed) {
        esp_err_t transition_err = firmware_update_transition(FIRMWARE_UPDATE_STATE_SUCCESS,
                                                              100,
                                                              err,
                                                              CONFIG_FIRMWARE_VERSION_STRING);
        if (transition_err != ESP_OK) {
            return transition_err;
        }
    }
    return err;
}

esp_err_t firmware_update_rollback_and_reboot(esp_err_t error)
{
    return firmware_boot_check_rollback_and_reboot(error);
}

esp_err_t firmware_update_reboot(void)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!firmware_update_claim_operation()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = firmware_update_transition(FIRMWARE_UPDATE_STATE_REBOOTING,
                                                100,
                                                ESP_OK,
                                                NULL);
    if (err != ESP_OK) {
        firmware_update_release_operation();
        return err;
    }

    BaseType_t created = xTaskCreate(firmware_update_reboot_task,
                                     "ota_reboot",
                                     FIRMWARE_UPDATE_REBOOT_TASK_STACK_SIZE,
                                     NULL,
                                     FIRMWARE_UPDATE_REBOOT_TASK_PRIORITY,
                                     NULL);
    if (created != pdPASS) {
        err = ESP_ERR_NO_MEM;
        firmware_update_transition(FIRMWARE_UPDATE_STATE_FAILED, 100, err, NULL);
        firmware_update_release_operation();
        return err;
    }
    return ESP_OK;
}

esp_err_t firmware_update_get_status(firmware_update_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    *status = s_status;
    xSemaphoreGive(s_status_lock);
    return ESP_OK;
}

esp_err_t firmware_update_get_history(firmware_update_history_t *history)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    return firmware_storage_get_history(history);
}

esp_err_t firmware_update_acknowledge_rollback(void)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    return firmware_storage_clear_rollback_notice();
}

esp_err_t firmware_update_prepare_factory_reset(void)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_factory_resetting = true;
    memset(&s_available_manifest, 0, sizeof(s_available_manifest));
    s_available_manifest_valid = false;
    xSemaphoreGive(s_status_lock);

    esp_err_t err = firmware_storage_clear_staged();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Factory reset erased staged firmware metadata");
    }
    return err;
}

esp_err_t firmware_update_get_available_manifest(firmware_manifest_t *manifest)
{
    if (!manifest) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    if (!s_available_manifest_valid ||
        s_status.state != FIRMWARE_UPDATE_STATE_AVAILABLE) {
        xSemaphoreGive(s_status_lock);
        return ESP_ERR_NOT_FOUND;
    }
    *manifest = s_available_manifest;
    xSemaphoreGive(s_status_lock);
    return ESP_OK;
}

bool firmware_update_is_busy(void)
{
    if (!s_status_lock) {
        return false;
    }

    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    bool busy = firmware_update_state_is_busy(s_status.state);
    xSemaphoreGive(s_status_lock);
    return busy;
}

esp_err_t firmware_update_transition(firmware_update_state_t state,
                                     uint8_t progress,
                                     esp_err_t error,
                                     const char *target_version)
{
    if (!s_status_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (progress > 100) {
        return ESP_ERR_INVALID_ARG;
    }

    firmware_update_status_t snapshot;
    xSemaphoreTake(s_status_lock, portMAX_DELAY);

    firmware_update_state_t previous = s_status.state;
    if (!firmware_update_transition_allowed(previous, state)) {
        xSemaphoreGive(s_status_lock);
        ESP_LOGW(TAG,
                 "Rejected state transition: %s -> %s",
                 firmware_update_state_to_string(previous),
                 firmware_update_state_to_string(state));
        return ESP_ERR_INVALID_STATE;
    }

    s_status.state = state;
    s_status.progress = progress;
    s_status.last_error = error;
    s_status.revision++;

    if (state == FIRMWARE_UPDATE_STATE_IDLE ||
        state == FIRMWARE_UPDATE_STATE_CHECKING) {
        s_status.target_version[0] = '\0';
    } else if (target_version) {
        strlcpy(s_status.target_version,
                target_version,
                sizeof(s_status.target_version));
    }

    snapshot = s_status;
    xSemaphoreGive(s_status_lock);

    ESP_LOGI(TAG,
             "State: %s -> %s, progress=%u, error=%s",
             firmware_update_state_to_string(previous),
             firmware_update_state_to_string(state),
             (unsigned)progress,
             esp_err_to_name(error));

    esp_err_t event_err = esp_event_post(FIRMWARE_UPDATE_EVENT,
                                         FIRMWARE_UPDATE_EVENT_STATUS_CHANGED,
                                         &snapshot,
                                         sizeof(snapshot),
                                         pdMS_TO_TICKS(100));
    if (event_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish status event: %s", esp_err_to_name(event_err));
    }
    return ESP_OK;
}

static void firmware_update_check_task(void *argument)
{
    firmware_update_check_args_t args = *(firmware_update_check_args_t *)argument;
    free(argument);

    const esp_partition_t *update_partition = NULL;
    esp_err_t err = firmware_update_check_prerequisites("Checking updates",
                                                        OTA_INFO_URL,
                                                        &update_partition);
    char *manifest_json = NULL;
    size_t manifest_length = 0;
    firmware_manifest_t manifest = {0};
    int version_comparison = 0;

    if (err != ESP_OK) {
        goto failed;
    }

    const size_t manifest_capacity = FIRMWARE_MANIFEST_MAX_SIZE + 1;
    manifest_json = heap_caps_malloc(manifest_capacity,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!manifest_json) {
        ESP_LOGE(TAG,
                 "Failed to allocate %u-byte Manifest buffer in PSRAM",
                 (unsigned)manifest_capacity);
        err = ESP_ERR_NO_MEM;
        goto failed;
    }

    ESP_LOGI(TAG,
             "Manifest buffer allocated in PSRAM: size=%u internal_free=%u "
             "internal_largest=%u",
             (unsigned)manifest_capacity,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    err = firmware_manifest_download(OTA_INFO_URL,
                                     manifest_json,
                                     manifest_capacity,
                                     &manifest_length);
    if (err != ESP_OK) {
        goto failed;
    }

    err = firmware_manifest_parse(manifest_json, manifest_length, &manifest);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Downloaded Manifest validation failed: %s", esp_err_to_name(err));
        goto failed;
    }

    err = firmware_manifest_validate_for_device(&manifest,
                                                OTA_DEFAULT_CHANNEL_STRING,
                                                update_partition->size);
    if (err != ESP_OK) {
        goto failed;
    }

    const char *comparison_version = args.preserve_staged
                                         ? args.staged.version
                                         : CONFIG_FIRMWARE_VERSION_STRING;
    err = firmware_manifest_compare_running_version(&manifest,
                                                    comparison_version,
                                                    &version_comparison);
    if (err != ESP_OK) {
        goto failed;
    }

    heap_caps_free(manifest_json);
    manifest_json = NULL;

    if (version_comparison <= 0) {
        if (args.preserve_staged) {
            ESP_LOGI(TAG,
                     "No newer update available: staged=%s manifest=%s",
                     args.staged.version,
                     manifest.version);
            firmware_update_transition(FIRMWARE_UPDATE_STATE_STAGED,
                                       100,
                                       ESP_OK,
                                       args.staged.version);
        }
        else {
            ESP_LOGI(TAG,
                     "No update available: running=%s manifest=%s",
                     CONFIG_FIRMWARE_VERSION_STRING,
                     manifest.version);
            firmware_update_transition(FIRMWARE_UPDATE_STATE_UP_TO_DATE,
                                       100,
                                       ESP_OK,
                                       NULL);
        }
    }
    else {
        firmware_update_store_available_manifest(&manifest);
        ESP_LOGI(TAG,
                 "Update available: baseline=%s target=%s size=%lu channel=%s",
                 comparison_version,
                 manifest.version,
                 (unsigned long)manifest.firmware_size,
                 manifest.channel);
        firmware_update_transition(FIRMWARE_UPDATE_STATE_AVAILABLE,
                                   100,
                                   ESP_OK,
                                   manifest.version);
    }

    firmware_update_release_operation();
    vTaskDeleteWithCaps(NULL);
    return;

failed:
    heap_caps_free(manifest_json);
    firmware_update_clear_available_manifest();
    ESP_LOGE(TAG, "Update check failed: %s", esp_err_to_name(err));
    if (args.preserve_staged) {
        firmware_update_transition(FIRMWARE_UPDATE_STATE_STAGED,
                                   100,
                                   err,
                                   args.staged.version);
    }
    else {
        firmware_update_transition(FIRMWARE_UPDATE_STATE_FAILED, 0, err, NULL);
    }
    firmware_update_release_operation();
    vTaskDeleteWithCaps(NULL);
}

static void firmware_update_install_task(void *argument)
{
    firmware_update_install_args_t args = *(firmware_update_install_args_t *)argument;
    free(argument);
    firmware_manifest_t manifest = args.manifest;

    const esp_partition_t *update_partition = NULL;
    esp_err_t err = firmware_update_check_prerequisites("Staging update",
                                                        manifest.firmware_url,
                                                        &update_partition);
    if (err == ESP_OK && manifest.firmware_size > update_partition->size) {
        ESP_LOGE(TAG,
                 "Firmware does not fit OTA partition: image=%lu partition=%lu",
                 (unsigned long)manifest.firmware_size,
                 (unsigned long)update_partition->size);
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK) {
        err = firmware_download_stage(&manifest,
                                      update_partition,
                                      firmware_update_install_progress,
                                      NULL);
    }
    if (err == ESP_OK) {
        xSemaphoreTake(s_status_lock, portMAX_DELAY);
        if (s_factory_resetting) {
            err = ESP_ERR_INVALID_STATE;
        }
        else {
            err = firmware_storage_record_staged(&manifest,
                                                 update_partition,
                                                 args.automatic);
        }
        xSemaphoreGive(s_status_lock);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to persist staged image metadata: %s", esp_err_to_name(err));
        }
    }

    if (err == ESP_OK) {
        firmware_update_transition(FIRMWARE_UPDATE_STATE_STAGED,
                                   100,
                                   ESP_OK,
                                   manifest.version);
    }
    else {
        firmware_storage_clear_staged();
        ESP_LOGE(TAG, "Firmware staging failed: %s", esp_err_to_name(err));
        firmware_update_transition(FIRMWARE_UPDATE_STATE_FAILED,
                                   0,
                                   err,
                                   manifest.version);
    }

    firmware_update_release_operation();
    vTaskDelete(NULL);
}

static void firmware_update_activate_task(void *argument)
{
    firmware_update_staged_info_t staged = *(firmware_update_staged_info_t *)argument;
    free(argument);

    const esp_partition_t *partition = firmware_update_find_staged_partition(&staged);
    esp_err_t err = partition ? ESP_OK : ESP_ERR_NOT_FOUND;
    if (err == ESP_OK) {
        err = firmware_download_validate_staged(&staged, partition);
    }
    if (err == ESP_OK) {
        err = firmware_storage_begin_update(CONFIG_FIRMWARE_VERSION_STRING,
                                            staged.version);
    }
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(partition);
        if (err != ESP_OK) {
            firmware_storage_record_install_failure(err);
        }
    }

    if (err == ESP_OK) {
        esp_err_t clear_err = firmware_storage_clear_staged();
        if (clear_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to clear staged metadata after activation: %s",
                     esp_err_to_name(clear_err));
        }
        ESP_LOGI(TAG,
                 "Staged firmware activated: version=%s partition=%s",
                 staged.version,
                 partition->label);
        firmware_update_transition(FIRMWARE_UPDATE_STATE_READY_TO_REBOOT,
                                   100,
                                   ESP_OK,
                                   staged.version);
    }
    else {
        ESP_LOGE(TAG, "Firmware activation failed: %s", esp_err_to_name(err));
        firmware_update_transition(FIRMWARE_UPDATE_STATE_STAGED,
                                   100,
                                   err,
                                   staged.version);
    }

    firmware_update_release_operation();
    vTaskDelete(NULL);
}

static void firmware_update_reboot_task(void *argument)
{
    (void)argument;

    ESP_LOGI(TAG, "Restarting into the installed OTA image");
    vTaskDelay(pdMS_TO_TICKS(FIRMWARE_UPDATE_REBOOT_DELAY_MS));
    esp_restart();
    vTaskDelete(NULL);
}

static void firmware_update_install_progress(firmware_download_stage_t stage,
                                             uint8_t progress,
                                             void *user_context)
{
    (void)user_context;

    firmware_update_state_t state = stage == FIRMWARE_DOWNLOAD_STAGE_VERIFYING
                                        ? FIRMWARE_UPDATE_STATE_VERIFYING
                                        : FIRMWARE_UPDATE_STATE_DOWNLOADING;
    firmware_update_transition(state, progress, ESP_OK, NULL);
}

static bool firmware_update_claim_operation(void)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    bool factory_resetting = s_factory_resetting;
    xSemaphoreGive(s_status_lock);
    if (factory_resetting) {
        return false;
    }

    taskENTER_CRITICAL(&s_operation_guard);
    bool claimed = !s_operation_claimed;
    if (claimed) {
        s_operation_claimed = true;
    }
    taskEXIT_CRITICAL(&s_operation_guard);
    return claimed;
}

static void firmware_update_release_operation(void)
{
    taskENTER_CRITICAL(&s_operation_guard);
    s_operation_claimed = false;
    taskEXIT_CRITICAL(&s_operation_guard);
}

static void firmware_update_clear_available_manifest(void)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    memset(&s_available_manifest, 0, sizeof(s_available_manifest));
    s_available_manifest_valid = false;
    xSemaphoreGive(s_status_lock);
}

static void firmware_update_store_available_manifest(const firmware_manifest_t *manifest)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_available_manifest = *manifest;
    s_available_manifest_valid = true;
    xSemaphoreGive(s_status_lock);
}

static esp_err_t firmware_update_check_prerequisites(const char *operation,
                                                     const char *request_url,
                                                     const esp_partition_t **update_partition)
{
    if (!operation || !request_url || !update_partition) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkState *network = network_get_state();
    if (!network || (!network->sta_connected_flag && !network->eth_connected)) {
        ESP_LOGE(TAG, "Cannot check for updates without an uplink connection");
        return ESP_ERR_INVALID_STATE;
    }

    bool request_uses_tls = strncmp(request_url,
                                    "https://",
                                    strlen("https://")) == 0;
    if (request_uses_tls) {
        time_t now = 0;
        time(&now);
        if (now < FIRMWARE_UPDATE_VALID_TIME_MINIMUM) {
            ESP_LOGE(TAG,
                     "Cannot verify OTA TLS certificates before system time is synchronized");
            return ESP_ERR_INVALID_STATE;
        }
    }

    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (!partition || partition->size == 0) {
        ESP_LOGE(TAG, "No writable OTA application partition is available");
        return ESP_ERR_NOT_FOUND;
    }

    *update_partition = partition;
    ESP_LOGI(TAG,
             "%s: partition=%s size=%lu channel=%s",
             operation,
             partition->label,
             (unsigned long)partition->size,
             OTA_DEFAULT_CHANNEL_STRING);
    return ESP_OK;
}

static const esp_partition_t *firmware_update_find_staged_partition(
    const firmware_update_staged_info_t *staged)
{
    if (!staged || !staged->valid || staged->partition_label[0] == '\0') {
        return NULL;
    }
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_ANY,
        staged->partition_label);
    if (!partition || partition == esp_ota_get_running_partition() ||
        staged->image_size == 0 || staged->image_size > partition->size) {
        return NULL;
    }
    return partition;
}

const char *firmware_update_state_to_string(firmware_update_state_t state)
{
    switch (state) {
    case FIRMWARE_UPDATE_STATE_IDLE:
        return "idle";
    case FIRMWARE_UPDATE_STATE_CHECKING:
        return "checking";
    case FIRMWARE_UPDATE_STATE_UP_TO_DATE:
        return "up_to_date";
    case FIRMWARE_UPDATE_STATE_AVAILABLE:
        return "available";
    case FIRMWARE_UPDATE_STATE_DOWNLOADING:
        return "downloading";
    case FIRMWARE_UPDATE_STATE_VERIFYING:
        return "verifying";
    case FIRMWARE_UPDATE_STATE_STAGED:
        return "staged";
    case FIRMWARE_UPDATE_STATE_ACTIVATING:
        return "activating";
    case FIRMWARE_UPDATE_STATE_READY_TO_REBOOT:
        return "ready_to_reboot";
    case FIRMWARE_UPDATE_STATE_REBOOTING:
        return "rebooting";
    case FIRMWARE_UPDATE_STATE_BOOT_VERIFYING:
        return "boot_verifying";
    case FIRMWARE_UPDATE_STATE_SUCCESS:
        return "success";
    case FIRMWARE_UPDATE_STATE_ROLLED_BACK:
        return "rolled_back";
    case FIRMWARE_UPDATE_STATE_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}
