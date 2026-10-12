#include "firmware_auto_check.h"

#include <string.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "firmware_update.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "network.h"
#include "nvs_storage.h"
#include "sdkconfig.h"
#include "sys_time_api.h"

#define FIRMWARE_AUTO_CHECK_NAMESPACE "ota_schedule"
#define FIRMWARE_AUTO_CHECK_SCHEMA_VERSION 3

#define FIRMWARE_AUTO_CHECK_KEY_SCHEMA "schema"
#define FIRMWARE_AUTO_CHECK_KEY_ENABLED "enabled"
#define FIRMWARE_AUTO_CHECK_KEY_LAST_TIME "last_time"
#define FIRMWARE_AUTO_CHECK_KEY_LAST_DAILY_DAY "last_daily"
#define FIRMWARE_AUTO_CHECK_KEY_RESULT "last_result"
#define FIRMWARE_AUTO_CHECK_KEY_ERROR "last_error"
#define FIRMWARE_AUTO_CHECK_KEY_SCHEDULE_DAY "sched_day"
#define FIRMWARE_AUTO_CHECK_KEY_SCHEDULE_MINUTE "sched_min"

#define FIRMWARE_AUTO_CHECK_TASK_STACK_SIZE 4096
#define FIRMWARE_AUTO_CHECK_TASK_PRIORITY 4
#define FIRMWARE_AUTO_CHECK_POLL_MS 30000
#define FIRMWARE_AUTO_CHECK_ACTIVE_POLL_MS 1000
#define FIRMWARE_AUTO_CHECK_WIFI_SETTLE_MS 5000
#define FIRMWARE_AUTO_CHECK_VALID_TIME_MINIMUM 1577836800

static const char *TAG = "firmware_auto_check";

typedef enum {
    FIRMWARE_AUTO_CHECK_TRIGGER_NONE = 0,
    FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED,
    FIRMWARE_AUTO_CHECK_TRIGGER_DAILY,
} firmware_auto_check_trigger_t;

static SemaphoreHandle_t s_record_lock;
static TaskHandle_t s_scheduler_task;
static firmware_auto_check_record_t s_record;
static bool s_initialized;
static bool s_factory_resetting;
static bool s_workflow_in_flight;
static bool s_wifi_link_connected;
static bool s_wifi_ip_ready;
static bool s_wifi_check_pending;
static TickType_t s_wifi_settle_start_tick;
static uint32_t s_wifi_connection_generation;
static uint32_t s_pending_wifi_generation;
static uint32_t s_active_wifi_generation;
static uint32_t s_completed_wifi_generation;
static firmware_auto_check_trigger_t s_active_trigger;
static bool s_wifi_candidate_available;

static void firmware_auto_check_task(void *argument);
static void firmware_auto_check_event_handler(void *argument,
                                              esp_event_base_t event_base,
                                              int32_t event_id,
                                              void *event_data);
static esp_err_t firmware_auto_check_load(void);
static esp_err_t firmware_auto_check_write_locked(void);
static void firmware_auto_check_run_cycle(void);
static void firmware_auto_check_reconcile_result(void);
static void firmware_auto_check_record_progress(firmware_auto_check_result_t result,
                                                esp_err_t error);
static void firmware_auto_check_finish(firmware_auto_check_result_t result,
                                       esp_err_t error);
static bool firmware_auto_check_get_local_clock(uint32_t *local_day,
                                                uint16_t *local_minute,
                                                uint32_t *utc_time);
static bool firmware_auto_check_state_is_eligible(firmware_update_state_t state);
static const char *firmware_auto_check_trigger_to_string(
    firmware_auto_check_trigger_t trigger);

esp_err_t firmware_auto_check_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_record_lock = xSemaphoreCreateMutex();
    if (!s_record_lock) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = firmware_auto_check_load();
    if (err != ESP_OK) {
        vSemaphoreDelete(s_record_lock);
        s_record_lock = NULL;
        return err;
    }

    s_wifi_link_connected = false;
    s_wifi_ip_ready = false;
    s_wifi_check_pending = false;
    s_wifi_settle_start_tick = 0;
    s_wifi_connection_generation = 0;
    s_pending_wifi_generation = 0;
    s_active_wifi_generation = 0;
    s_completed_wifi_generation = 0;
    s_active_trigger = FIRMWARE_AUTO_CHECK_TRIGGER_NONE;
    s_wifi_candidate_available = false;
    s_factory_resetting = false;
    NetworkState *network = network_get_state();
    if (OTA_AUTO_CHECK_ON_WIFI_CONNECT && network &&
        network->sta_connected_flag && network->current_ip.is_valid) {
        s_wifi_link_connected = true;
        s_wifi_ip_ready = true;
        s_wifi_check_pending = true;
        s_wifi_connection_generation = 1;
        s_pending_wifi_generation = 1;
        s_wifi_settle_start_tick = xTaskGetTickCount();
    }

    err = esp_event_handler_register(SYS_TIME_EVENT,
                                     SYS_TIME_EVENT_SYNCED,
                                     firmware_auto_check_event_handler,
                                     NULL);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_record_lock);
        s_record_lock = NULL;
        return err;
    }
    err = esp_event_handler_register(FIRMWARE_UPDATE_EVENT,
                                     FIRMWARE_UPDATE_EVENT_STATUS_CHANGED,
                                     firmware_auto_check_event_handler,
                                     NULL);
    if (err != ESP_OK) {
        esp_event_handler_unregister(SYS_TIME_EVENT,
                                     SYS_TIME_EVENT_SYNCED,
                                     firmware_auto_check_event_handler);
        vSemaphoreDelete(s_record_lock);
        s_record_lock = NULL;
        return err;
    }

    BaseType_t created = xTaskCreate(firmware_auto_check_task,
                                     "ota_auto_check",
                                     FIRMWARE_AUTO_CHECK_TASK_STACK_SIZE,
                                     NULL,
                                     FIRMWARE_AUTO_CHECK_TASK_PRIORITY,
                                     &s_scheduler_task);
    if (created != pdPASS) {
        esp_event_handler_unregister(FIRMWARE_UPDATE_EVENT,
                                     FIRMWARE_UPDATE_EVENT_STATUS_CHANGED,
                                     firmware_auto_check_event_handler);
        esp_event_handler_unregister(SYS_TIME_EVENT,
                                     SYS_TIME_EVENT_SYNCED,
                                     firmware_auto_check_event_handler);
        vSemaphoreDelete(s_record_lock);
        s_record_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG,
             "Initialized: enabled=%d check_on_wifi_connect=%d wifi_settle=%dms "
             "window=%02d:00+%dmin schedule_day=%lu "
             "schedule=%02u:%02u last_day=%lu last_result=%d last_error=%s",
             s_record.enabled,
             OTA_AUTO_CHECK_ON_WIFI_CONNECT,
             FIRMWARE_AUTO_CHECK_WIFI_SETTLE_MS,
             OTA_AUTO_CHECK_START_HOUR,
             OTA_AUTO_CHECK_WINDOW_MINUTES,
             (unsigned long)s_record.scheduled_local_day,
             s_record.scheduled_minute / 60U,
             s_record.scheduled_minute % 60U,
             (unsigned long)s_record.last_daily_check_local_day,
             s_record.last_result,
             esp_err_to_name(s_record.last_error));
    xTaskNotifyGive(s_scheduler_task);
    return ESP_OK;
}

esp_err_t firmware_auto_check_get_record(firmware_auto_check_record_t *record)
{
    if (!record) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_record_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    *record = s_record;
    xSemaphoreGive(s_record_lock);
    return ESP_OK;
}

esp_err_t firmware_auto_check_set_enabled(bool enabled)
{
    if (!s_initialized || !s_record_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    s_record.enabled = enabled;
    esp_err_t err = firmware_auto_check_write_locked();
    xSemaphoreGive(s_record_lock);
    if (err == ESP_OK && s_scheduler_task) {
        xTaskNotifyGive(s_scheduler_task);
    }
    return err;
}

esp_err_t firmware_auto_check_prepare_factory_reset(void)
{
    if (!s_initialized || !s_record_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    s_factory_resetting = true;
    s_record.enabled = false;
    s_wifi_check_pending = false;
    s_pending_wifi_generation = 0;
    s_wifi_candidate_available = false;
    xSemaphoreGive(s_record_lock);

    esp_err_t err = nvs_storage_erase_namespace(FIRMWARE_AUTO_CHECK_NAMESPACE);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Factory reset erased automatic OTA schedule");
    }
    return err;
}

bool firmware_auto_check_has_wifi_candidate(void)
{
    if (!s_initialized || !s_record_lock) {
        return false;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    bool available = s_wifi_candidate_available;
    xSemaphoreGive(s_record_lock);
    return available;
}

void firmware_auto_check_notify_wifi_connected(void)
{
    if (!s_initialized || !s_record_lock || !OTA_AUTO_CHECK_ON_WIFI_CONNECT) {
        return;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    s_wifi_connection_generation++;
    if (s_wifi_connection_generation == 0) {
        s_wifi_connection_generation = 1;
    }
    s_wifi_link_connected = true;
    s_wifi_ip_ready = false;
    s_wifi_check_pending = false;
    s_pending_wifi_generation = 0;
    xSemaphoreGive(s_record_lock);
}

void firmware_auto_check_notify_wifi_got_ip(void)
{
    if (!s_initialized || !s_record_lock || !OTA_AUTO_CHECK_ON_WIFI_CONNECT) {
        return;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    if (!s_wifi_link_connected) {
        /* Recover if the association event happened before this component was
         * initialized or was otherwise not observed. */
        s_wifi_connection_generation++;
        if (s_wifi_connection_generation == 0) {
            s_wifi_connection_generation = 1;
        }
        s_wifi_link_connected = true;
    }
    s_wifi_ip_ready = true;
    uint32_t generation = s_wifi_connection_generation;
    bool already_tracked = generation == s_pending_wifi_generation ||
                           generation == s_active_wifi_generation ||
                           generation == s_completed_wifi_generation;
    if (!already_tracked) {
        s_wifi_check_pending = true;
        s_pending_wifi_generation = generation;
        s_wifi_settle_start_tick = xTaskGetTickCount();
        ESP_LOGI(TAG,
                 "Wi-Fi connection check queued: generation=%lu settle=%dms",
                 (unsigned long)generation,
                 FIRMWARE_AUTO_CHECK_WIFI_SETTLE_MS);
    }
    xSemaphoreGive(s_record_lock);

    if (s_scheduler_task) {
        xTaskNotifyGive(s_scheduler_task);
    }
}

void firmware_auto_check_notify_wifi_disconnected(void)
{
    if (!s_initialized || !s_record_lock || !OTA_AUTO_CHECK_ON_WIFI_CONNECT) {
        return;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    s_wifi_link_connected = false;
    s_wifi_ip_ready = false;
    if (s_wifi_check_pending &&
        s_pending_wifi_generation == s_wifi_connection_generation) {
        ESP_LOGI(TAG,
                 "Pending Wi-Fi connection check canceled: generation=%lu",
                 (unsigned long)s_pending_wifi_generation);
        s_wifi_check_pending = false;
        s_pending_wifi_generation = 0;
    }
    xSemaphoreGive(s_record_lock);

    if (s_scheduler_task) {
        xTaskNotifyGive(s_scheduler_task);
    }
}

static void firmware_auto_check_task(void *argument)
{
    (void)argument;
    while (true) {
        xSemaphoreTake(s_record_lock, portMAX_DELAY);
        bool workflow_in_flight = s_workflow_in_flight;
        bool wifi_settling = false;
        if (s_wifi_check_pending && s_wifi_ip_ready) {
            TickType_t settle_ticks = pdMS_TO_TICKS(FIRMWARE_AUTO_CHECK_WIFI_SETTLE_MS);
            wifi_settling = xTaskGetTickCount() - s_wifi_settle_start_tick < settle_ticks;
        }
        xSemaphoreGive(s_record_lock);
        uint32_t poll_ms = workflow_in_flight || wifi_settling
                               ? FIRMWARE_AUTO_CHECK_ACTIVE_POLL_MS
                               : FIRMWARE_AUTO_CHECK_POLL_MS;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(poll_ms));
        firmware_auto_check_reconcile_result();
        firmware_auto_check_run_cycle();
    }
}

static void firmware_auto_check_event_handler(void *argument,
                                              esp_event_base_t event_base,
                                              int32_t event_id,
                                              void *event_data)
{
    (void)argument;
    if (event_base == SYS_TIME_EVENT && event_id == SYS_TIME_EVENT_SYNCED &&
        s_record_lock) {
        xSemaphoreTake(s_record_lock, portMAX_DELAY);
        if (s_wifi_check_pending && s_wifi_ip_ready) {
            /* Start the settling period after both network and local time are
             * ready, whichever condition became ready last. */
            s_wifi_settle_start_tick = xTaskGetTickCount();
        }
        xSemaphoreGive(s_record_lock);
    }
    else if (event_base == FIRMWARE_UPDATE_EVENT &&
             event_id == FIRMWARE_UPDATE_EVENT_STATUS_CHANGED &&
             event_data && s_record_lock) {
        const firmware_update_status_t *status = event_data;
        if (status->state != FIRMWARE_UPDATE_STATE_AVAILABLE) {
            xSemaphoreTake(s_record_lock, portMAX_DELAY);
            s_wifi_candidate_available = false;
            xSemaphoreGive(s_record_lock);
        }
    }
    if (s_scheduler_task) {
        xTaskNotifyGive(s_scheduler_task);
    }
}

static esp_err_t firmware_auto_check_load(void)
{
    uint32_t schema = 0;
    uint8_t enabled = OTA_AUTO_CHECK_DEFAULT ? 1 : 0;
    uint8_t result = FIRMWARE_AUTO_CHECK_RESULT_NONE;
    uint32_t last_time = 0;
    uint32_t last_daily_day = 0;
    uint32_t last_error = ESP_OK;
    uint32_t schedule_day = 0;
    uint32_t schedule_minute = 0;
    nvs_storage_read_item_t items[] = {
        {.key = FIRMWARE_AUTO_CHECK_KEY_SCHEMA, .type = NVS_STORAGE_TYPE_U32, .value = &schema},
        {.key = FIRMWARE_AUTO_CHECK_KEY_ENABLED, .type = NVS_STORAGE_TYPE_U8, .value = &enabled},
        {.key = FIRMWARE_AUTO_CHECK_KEY_LAST_TIME, .type = NVS_STORAGE_TYPE_U32, .value = &last_time},
        {.key = FIRMWARE_AUTO_CHECK_KEY_LAST_DAILY_DAY,
         .type = NVS_STORAGE_TYPE_U32,
         .value = &last_daily_day},
        {.key = FIRMWARE_AUTO_CHECK_KEY_RESULT, .type = NVS_STORAGE_TYPE_U8, .value = &result},
        {.key = FIRMWARE_AUTO_CHECK_KEY_ERROR, .type = NVS_STORAGE_TYPE_U32, .value = &last_error},
        {.key = FIRMWARE_AUTO_CHECK_KEY_SCHEDULE_DAY, .type = NVS_STORAGE_TYPE_U32, .value = &schedule_day},
        {.key = FIRMWARE_AUTO_CHECK_KEY_SCHEDULE_MINUTE, .type = NVS_STORAGE_TYPE_U32, .value = &schedule_minute},
    };

    bool valid = false;
    esp_err_t err = nvs_storage_read(FIRMWARE_AUTO_CHECK_NAMESPACE,
                                     items,
                                     sizeof(items) / sizeof(items[0]));
    if (err == ESP_OK) {
        valid = true;
        for (size_t index = 0; index < sizeof(items) / sizeof(items[0]); index++) {
            if (items[index].result != ESP_OK) {
                valid = false;
                break;
            }
        }
        valid = valid && schema == FIRMWARE_AUTO_CHECK_SCHEMA_VERSION &&
                enabled <= 1 &&
                result <= FIRMWARE_AUTO_CHECK_RESULT_DOWNLOAD_FAILED &&
                schedule_minute < 1440;
    }

    memset(&s_record, 0, sizeof(s_record));
    if (valid) {
        s_record.enabled = enabled != 0;
        s_record.last_check_time = last_time;
        s_record.last_daily_check_local_day = last_daily_day;
        s_record.last_result = (firmware_auto_check_result_t)result;
        s_record.last_error = (esp_err_t)last_error;
        s_record.scheduled_local_day = schedule_day;
        s_record.scheduled_minute = (uint16_t)schedule_minute;
        if (s_record.last_result == FIRMWARE_AUTO_CHECK_RESULT_STARTED) {
            /* An asynchronous check cannot survive a restart. Preserve any
             * daily marker already written, but make the result terminal. */
            s_record.last_result = FIRMWARE_AUTO_CHECK_RESULT_CHECK_FAILED;
            s_record.last_error = ESP_ERR_INVALID_STATE;
            return firmware_auto_check_write_locked();
        }
        if (s_record.last_result == FIRMWARE_AUTO_CHECK_RESULT_DOWNLOADING) {
            firmware_update_status_t status = {0};
            if (firmware_update_get_status(&status) == ESP_OK &&
                status.state == FIRMWARE_UPDATE_STATE_STAGED) {
                s_record.last_result = FIRMWARE_AUTO_CHECK_RESULT_STAGED;
                s_record.last_error = ESP_OK;
            }
            else {
                /* An incomplete OTA write cannot be resumed after restart. A
                 * fully persisted staged image is recovered by
                 * firmware_update_init() before this component starts.
                 * UPDATE_AVAILABLE is a valid terminal result for a
                 * Manifest-only Wi-Fi connection check and is preserved. */
                s_record.last_result = FIRMWARE_AUTO_CHECK_RESULT_DOWNLOAD_FAILED;
                s_record.last_error = ESP_ERR_INVALID_STATE;
            }
            return firmware_auto_check_write_locked();
        }
        return ESP_OK;
    }

    s_record.enabled = OTA_AUTO_CHECK_DEFAULT != 0;
    s_record.last_result = FIRMWARE_AUTO_CHECK_RESULT_NONE;
    s_record.last_error = ESP_OK;
    return firmware_auto_check_write_locked();
}

static esp_err_t firmware_auto_check_write_locked(void)
{
    if (s_factory_resetting) {
        return ESP_OK;
    }

    const nvs_storage_write_item_t items[] = {
        {.key = FIRMWARE_AUTO_CHECK_KEY_SCHEMA,
         .type = NVS_STORAGE_TYPE_U32,
         .value.u32 = FIRMWARE_AUTO_CHECK_SCHEMA_VERSION},
        {.key = FIRMWARE_AUTO_CHECK_KEY_ENABLED,
         .type = NVS_STORAGE_TYPE_U8,
         .value.u8 = s_record.enabled ? 1 : 0},
        {.key = FIRMWARE_AUTO_CHECK_KEY_LAST_TIME,
         .type = NVS_STORAGE_TYPE_U32,
         .value.u32 = s_record.last_check_time},
        {.key = FIRMWARE_AUTO_CHECK_KEY_LAST_DAILY_DAY,
         .type = NVS_STORAGE_TYPE_U32,
         .value.u32 = s_record.last_daily_check_local_day},
        {.key = FIRMWARE_AUTO_CHECK_KEY_RESULT,
         .type = NVS_STORAGE_TYPE_U8,
         .value.u8 = (uint8_t)s_record.last_result},
        {.key = FIRMWARE_AUTO_CHECK_KEY_ERROR,
         .type = NVS_STORAGE_TYPE_U32,
         .value.u32 = (uint32_t)s_record.last_error},
        {.key = FIRMWARE_AUTO_CHECK_KEY_SCHEDULE_DAY,
         .type = NVS_STORAGE_TYPE_U32,
         .value.u32 = s_record.scheduled_local_day},
        {.key = FIRMWARE_AUTO_CHECK_KEY_SCHEDULE_MINUTE,
         .type = NVS_STORAGE_TYPE_U32,
         .value.u32 = s_record.scheduled_minute},
    };
    return nvs_storage_write(FIRMWARE_AUTO_CHECK_NAMESPACE,
                             items,
                             sizeof(items) / sizeof(items[0]));
}

static void firmware_auto_check_run_cycle(void)
{
    uint32_t local_day = 0;
    uint16_t local_minute = 0;
    uint32_t utc_time = 0;
    if (!firmware_auto_check_get_local_clock(&local_day, &local_minute, &utc_time)) {
        return;
    }

    const uint16_t daily_window_end =
        (uint16_t)(OTA_AUTO_CHECK_START_HOUR * 60U +
                   OTA_AUTO_CHECK_WINDOW_MINUTES);

    NetworkState *network = network_get_state();
    bool wifi_connected = network && network->sta_connected_flag;
    bool uplink_connected = network &&
                            (network->sta_connected_flag || network->eth_connected);

    firmware_auto_check_trigger_t trigger = FIRMWARE_AUTO_CHECK_TRIGGER_NONE;
    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    if (s_factory_resetting || !s_record.enabled || s_workflow_in_flight) {
        xSemaphoreGive(s_record_lock);
        return;
    }

    if (s_record.scheduled_local_day != local_day) {
        uint32_t start_minute = OTA_AUTO_CHECK_START_HOUR * 60U;
        s_record.scheduled_local_day = local_day;
        s_record.scheduled_minute = (uint16_t)(start_minute +
            esp_random() % OTA_AUTO_CHECK_WINDOW_MINUTES);
        esp_err_t schedule_err = firmware_auto_check_write_locked();
        if (schedule_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to persist daily schedule: %s", esp_err_to_name(schedule_err));
            xSemaphoreGive(s_record_lock);
            return;
        }
        ESP_LOGI(TAG,
                 "Scheduled daily check: day=%lu time=%02u:%02u",
                 (unsigned long)local_day,
                 s_record.scheduled_minute / 60U,
                 s_record.scheduled_minute % 60U);
    }

    if (s_wifi_check_pending) {
        TickType_t settle_ticks = pdMS_TO_TICKS(FIRMWARE_AUTO_CHECK_WIFI_SETTLE_MS);
        bool settled = s_wifi_link_connected && s_wifi_ip_ready &&
                       xTaskGetTickCount() - s_wifi_settle_start_tick >= settle_ticks;
        if (settled) {
            trigger = FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED;
        }
    }
    /* A daily download may start only inside the configured window. Do not
     * replay a missed overnight job when the device comes online later. */
    else if (s_record.last_daily_check_local_day != local_day &&
             local_minute >= s_record.scheduled_minute &&
             local_minute < daily_window_end) {
        trigger = FIRMWARE_AUTO_CHECK_TRIGGER_DAILY;
    }

    if (trigger == FIRMWARE_AUTO_CHECK_TRIGGER_NONE) {
        xSemaphoreGive(s_record_lock);
        return;
    }
    xSemaphoreGive(s_record_lock);

    bool trigger_network_ready =
        trigger == FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED
            ? wifi_connected
            : uplink_connected;
    if (!trigger_network_ready) {
        return;
    }

    firmware_update_status_t status = {0};
    if (firmware_update_get_status(&status) != ESP_OK ||
        !firmware_auto_check_state_is_eligible(status.state)) {
        return;
    }

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    bool trigger_still_due = false;
    uint32_t wifi_generation = 0;
    if (trigger == FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED) {
        TickType_t settle_ticks = pdMS_TO_TICKS(FIRMWARE_AUTO_CHECK_WIFI_SETTLE_MS);
        trigger_still_due = s_wifi_check_pending &&
                            s_wifi_link_connected &&
                            s_wifi_ip_ready &&
                            xTaskGetTickCount() - s_wifi_settle_start_tick >= settle_ticks;
        wifi_generation = s_pending_wifi_generation;
    }
    else if (trigger == FIRMWARE_AUTO_CHECK_TRIGGER_DAILY) {
        trigger_still_due = s_record.last_daily_check_local_day != local_day &&
                            local_minute >= s_record.scheduled_minute &&
                            local_minute < daily_window_end;
    }
    if (!s_record.enabled || s_workflow_in_flight || !trigger_still_due) {
        xSemaphoreGive(s_record_lock);
        return;
    }
    s_record.last_check_time = utc_time;
    if (trigger == FIRMWARE_AUTO_CHECK_TRIGGER_DAILY) {
        s_record.last_daily_check_local_day = local_day;
    }
    s_record.last_result = FIRMWARE_AUTO_CHECK_RESULT_STARTED;
    s_record.last_error = ESP_OK;
    esp_err_t record_err = firmware_auto_check_write_locked();
    if (record_err == ESP_OK) {
        s_workflow_in_flight = true;
        s_active_trigger = trigger;
        if (trigger == FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED) {
            s_wifi_check_pending = false;
            s_pending_wifi_generation = 0;
            s_active_wifi_generation = wifi_generation;
        }
    }
    xSemaphoreGive(s_record_lock);
    if (record_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to record automatic check: %s", esp_err_to_name(record_err));
        return;
    }

    esp_err_t err = firmware_update_check_async();
    if (err != ESP_OK) {
        xSemaphoreTake(s_record_lock, portMAX_DELAY);
        s_workflow_in_flight = false;
        s_active_trigger = FIRMWARE_AUTO_CHECK_TRIGGER_NONE;
        if (trigger == FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED) {
            s_completed_wifi_generation = s_active_wifi_generation;
            s_active_wifi_generation = 0;
        }
        s_record.last_result = FIRMWARE_AUTO_CHECK_RESULT_CHECK_FAILED;
        s_record.last_error = err;
        firmware_auto_check_write_locked();
        xSemaphoreGive(s_record_lock);
        ESP_LOGE(TAG, "Failed to start automatic update check: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG,
             "%s automatic firmware check started",
             firmware_auto_check_trigger_to_string(trigger));
}

static void firmware_auto_check_reconcile_result(void)
{
    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    bool in_flight = s_workflow_in_flight;
    firmware_auto_check_result_t previous_result = s_record.last_result;
    firmware_auto_check_trigger_t active_trigger = s_active_trigger;
    xSemaphoreGive(s_record_lock);
    if (!in_flight) {
        return;
    }

    firmware_update_status_t status = {0};
    if (firmware_update_get_status(&status) != ESP_OK) {
        return;
    }

    switch (status.state) {
    case FIRMWARE_UPDATE_STATE_UP_TO_DATE:
        firmware_auto_check_finish(FIRMWARE_AUTO_CHECK_RESULT_UP_TO_DATE,
                                   status.last_error);
        return;
    case FIRMWARE_UPDATE_STATE_AVAILABLE: {
        if (active_trigger == FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED) {
            xSemaphoreTake(s_record_lock, portMAX_DELAY);
            s_wifi_candidate_available = true;
            xSemaphoreGive(s_record_lock);
            ESP_LOGI(TAG,
                     "Wi-Fi connection check found an update; firmware download deferred until user upgrade");
            firmware_auto_check_finish(FIRMWARE_AUTO_CHECK_RESULT_UPDATE_AVAILABLE,
                                       ESP_OK);
            return;
        }

        firmware_auto_check_record_progress(FIRMWARE_AUTO_CHECK_RESULT_UPDATE_AVAILABLE,
                                            ESP_OK);
        esp_err_t install_err = firmware_update_install_automatic_async();
        if (install_err == ESP_OK) {
            firmware_auto_check_record_progress(FIRMWARE_AUTO_CHECK_RESULT_DOWNLOADING,
                                                ESP_OK);
            ESP_LOGI(TAG, "Automatic firmware download started");
        }
        else if (install_err != ESP_ERR_INVALID_STATE) {
            firmware_auto_check_finish(FIRMWARE_AUTO_CHECK_RESULT_DOWNLOAD_FAILED,
                                       install_err);
            ESP_LOGE(TAG,
                     "Failed to start automatic firmware download: %s",
                     esp_err_to_name(install_err));
        }
        return;
    }
    case FIRMWARE_UPDATE_STATE_DOWNLOADING:
    case FIRMWARE_UPDATE_STATE_VERIFYING:
        firmware_auto_check_record_progress(FIRMWARE_AUTO_CHECK_RESULT_DOWNLOADING,
                                            ESP_OK);
        return;
    case FIRMWARE_UPDATE_STATE_STAGED:
        firmware_auto_check_finish(FIRMWARE_AUTO_CHECK_RESULT_STAGED,
                                   status.last_error);
        return;
    case FIRMWARE_UPDATE_STATE_FAILED:
        firmware_auto_check_finish(
            previous_result == FIRMWARE_AUTO_CHECK_RESULT_STARTED
                ? FIRMWARE_AUTO_CHECK_RESULT_CHECK_FAILED
                : FIRMWARE_AUTO_CHECK_RESULT_DOWNLOAD_FAILED,
            status.last_error);
        return;
    default:
        return;
    }
}

static void firmware_auto_check_record_progress(firmware_auto_check_result_t result,
                                                esp_err_t error)
{
    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    if (!s_workflow_in_flight ||
        (s_record.last_result == result && s_record.last_error == error)) {
        xSemaphoreGive(s_record_lock);
        return;
    }

    s_record.last_result = result;
    s_record.last_error = error;
    esp_err_t err = firmware_auto_check_write_locked();
    xSemaphoreGive(s_record_lock);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to persist automatic workflow progress: %s",
                 esp_err_to_name(err));
    }
}

static void firmware_auto_check_finish(firmware_auto_check_result_t result,
                                       esp_err_t error)
{
    time_t now = 0;
    time(&now);

    xSemaphoreTake(s_record_lock, portMAX_DELAY);
    if (!s_workflow_in_flight) {
        xSemaphoreGive(s_record_lock);
        return;
    }
    firmware_auto_check_trigger_t trigger = s_active_trigger;
    s_workflow_in_flight = false;
    s_active_trigger = FIRMWARE_AUTO_CHECK_TRIGGER_NONE;
    if (trigger == FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED) {
        s_completed_wifi_generation = s_active_wifi_generation;
        s_active_wifi_generation = 0;
    }
    s_record.last_check_time = now >= FIRMWARE_AUTO_CHECK_VALID_TIME_MINIMUM
                                   ? (uint32_t)now
                                   : s_record.last_check_time;
    s_record.last_result = result;
    s_record.last_error = error;
    esp_err_t err = firmware_auto_check_write_locked();
    xSemaphoreGive(s_record_lock);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to persist automatic workflow result: %s",
                 esp_err_to_name(err));
    }
    else {
        ESP_LOGI(TAG,
                 "%s automatic workflow completed: result=%d error=%s",
                 firmware_auto_check_trigger_to_string(trigger),
                 result,
                 esp_err_to_name(error));
    }
}

static bool firmware_auto_check_get_local_clock(uint32_t *local_day,
                                                uint16_t *local_minute,
                                                uint32_t *utc_time)
{
    if (!local_day || !local_minute || !utc_time ||
        !sys_time_get_dst_sync_status()) {
        return false;
    }

    time_t now = 0;
    time(&now);
    if (now < FIRMWARE_AUTO_CHECK_VALID_TIME_MINIMUM) {
        return false;
    }

    struct tm *local = sys_time_get_local_time();
    if (!local) {
        return false;
    }
    *local_day = (uint32_t)(local->tm_year + 1900) * 10000U +
                 (uint32_t)(local->tm_mon + 1) * 100U +
                 (uint32_t)local->tm_mday;
    *local_minute = (uint16_t)(local->tm_hour * 60 + local->tm_min);
    *utc_time = (uint32_t)now;
    return true;
}

static bool firmware_auto_check_state_is_eligible(firmware_update_state_t state)
{
    switch (state) {
    case FIRMWARE_UPDATE_STATE_IDLE:
    case FIRMWARE_UPDATE_STATE_UP_TO_DATE:
    case FIRMWARE_UPDATE_STATE_AVAILABLE:
    case FIRMWARE_UPDATE_STATE_STAGED:
    case FIRMWARE_UPDATE_STATE_FAILED:
    case FIRMWARE_UPDATE_STATE_SUCCESS:
    case FIRMWARE_UPDATE_STATE_ROLLED_BACK:
        return true;
    default:
        return false;
    }
}

static const char *firmware_auto_check_trigger_to_string(
    firmware_auto_check_trigger_t trigger)
{
    switch (trigger) {
    case FIRMWARE_AUTO_CHECK_TRIGGER_WIFI_CONNECTED:
        return "WiFi";
    case FIRMWARE_AUTO_CHECK_TRIGGER_DAILY:
        return "Daily";
    case FIRMWARE_AUTO_CHECK_TRIGGER_NONE:
    default:
        return "Unknown";
    }
}
