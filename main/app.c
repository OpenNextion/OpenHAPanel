#include "app.h"

#include "bsp_lcd.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "cJSON.h"

// BSP & LVGL
#include "bsp_lcd.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

// Utils component headers
#include "check_ram.h"
#include "uuid4.h"
#include "cjson_helpers.h"
#include "nvs_storage.h"
#include "littlefs.h"

// 网络与 Web 服务相关组件
#include "network.h"
#include "network_api.h"
#include "webserver_config.h"
#include "webserver.h"
#include "sys_time.h"
#include "sys_time_api.h"
#include "mdns_init.h"
#include "screen.h"
#include "screen_api.h"
#include "screen_state.h"
#include "screen_ui.h"
#include "screen_night_mode.h"
#include "webserver_message_parse.h"
#include "webserver_screen.h"
#include "webserver_firmware_update.h"

#include "webserver_config_api.h"
#include "webserver_config_forget_password.h"
#include "network_wifi_sta.h"
#include "network_wifi_ap.h"
#include "key.h"
#include "ha_auth.h"
#include "ha_auth_api.h"
#include "ha_entity_store.h"
#include "ha_ws.h"
#include "ha_screen_light.h"
#include "login_session.h"
#include "firmware_update.h"
#include "firmware_auto_check.h"
#include "entity_handlers/light/light_favorite_colors_store.h"

static const char *TAG = "main_app";

typedef struct app_ha_registry_removal_job app_ha_registry_removal_job_t;

static ha_ws_client_t *s_ha_ws_client;
static ha_entity_store_t *s_ha_entity_store;
static ha_entity_listener_handle_t *s_ha_entity_listener;
static ha_ws_client_t *s_ha_ws_cleanup_pending;
static ha_entity_store_t *s_ha_entity_store_cleanup_pending;
static QueueHandle_t s_ha_data_reconcile_queue;
static QueueHandle_t s_ha_registry_removal_queue;
static SemaphoreHandle_t s_ha_registry_pending_lock;
static app_ha_registry_removal_job_t *s_ha_registry_pending_head;
static app_ha_registry_removal_job_t *s_ha_registry_pending_tail;
static ha_ws_client_t *s_ha_registry_subscription_client;
static char s_ha_data_hass_url[HA_AUTH_URL_MAX_LEN];
static char s_ha_data_token[HA_AUTH_TOKEN_MAX_LEN];
static portMUX_TYPE s_ha_data_state_guard = portMUX_INITIALIZER_UNLOCKED;
/* Tracks the HA WebSocket transport independently from HA authentication.
 * device_connected becomes true only after auth_ok, which is too late for
 * deciding whether a notification represents transport or token failure. */
static bool s_ha_transport_connected;
/* Set once an interface reports a usable IP. The HA data layer must not connect
 * before that, otherwise the WebSocket handshake fails with EHOSTUNREACH. */
static volatile bool s_ha_network_ip_ready;
static bool s_screen_state_ready;
static volatile bool s_screen_power_ready;
static bool s_notification_ha_state_initialized;
static bool s_notification_ha_was_configured;
static bool s_notification_firmware_staged;
static portMUX_TYPE s_manual_ota_web_guard = portMUX_INITIALIZER_UNLOCKED;
static bool s_manual_ota_web_lock;
static int64_t s_ha_data_start_not_before_us;

typedef enum {
    APP_HA_OTA_CONNECTION_NORMAL = 0,
    APP_HA_OTA_CONNECTION_SUSPENDED,
    APP_HA_OTA_CONNECTION_RESUMING,
} app_ha_ota_connection_state_t;

static app_ha_ota_connection_state_t s_ha_ota_connection_state;

typedef enum {
    APP_HA_DATA_REQUEST_RECONCILE = 1,
    APP_HA_DATA_REQUEST_STOP,
} app_ha_data_request_t;

struct app_ha_registry_removal_job {
    size_t entity_id_count;
    char **entity_ids;
    struct app_ha_registry_removal_job *next;
};

#define APP_HA_DATA_TASK_STACK_SIZE 6144
#define APP_HA_DATA_TASK_PRIORITY   5
#define APP_HA_REGISTRY_TASK_STACK_SIZE 4096
#define APP_HA_REGISTRY_TASK_PRIORITY   4
#define APP_HA_REGISTRY_SIGNAL_QUEUE_LENGTH 1
#define APP_HA_DATA_AUTH_SETTLE_MS  500
#define APP_LCD_FIRST_FRAME_TIMEOUT_MS 1000
#define APP_LCD_FLUSH_SETTLE_MS        50

static const uint32_t s_ha_data_retry_delays_ms[] = {500, 1000, 2000, 5000};

static esp_err_t app_ha_data_manager_init(void);
static esp_err_t app_screen_state_manager_init(void);
static void app_lcd_prepare_black_frame(void);
static void app_lcd_enable_after_first_frame(uint8_t brightness);
static void app_notification_manager_init(void);
static bool app_set_notification_active(screen_notification_type_t type, bool active);
static bool app_set_wifi_connected_toast_pending(bool pending);
static bool app_set_wifi_status_connected(bool connected);
static void app_reconcile_ha_notifications(void);
static void app_reconcile_firmware_ui(void);
static void app_reconcile_firmware_web_status(const firmware_update_status_t *status);
static void app_reconcile_ha_data_for_firmware_status(
    const firmware_update_status_t *status);
static bool app_finish_ha_ota_resume(void);
static screen_page_t app_screen_state_resolve_target(void);
static void app_screen_state_reconcile(void);
static void app_factory_reset_task(void *arg);
static esp_err_t app_ha_data_reconcile(void);
static esp_err_t app_ha_data_start(const ha_auth_connection_info_t *info);
static bool app_ha_data_stop(void);
static void app_ha_data_post_request(app_ha_data_request_t request);
static uint32_t app_ha_data_get_start_delay_ms(void);
static void app_ha_auth_event_handler(void *arg,
                                      esp_event_base_t event_base,
                                      int32_t event_id,
                                      void *event_data);
static void app_ha_data_task(void *arg);
static void app_ha_connection_cb(ha_ws_client_t *client, ha_ws_event_t event, void *user_ctx);
static void app_ha_entity_changed_cb(ha_entity_store_t *store,
                                     const ha_entity_changed_event_t *event,
                                     void *user_ctx);
static void app_ha_registry_event_cb(ha_ws_client_t *client,
                                     int subscription_id,
                                     const cJSON *event,
                                     void *user_ctx);
static void app_ha_registry_subscription_result_cb(ha_ws_client_t *client,
                                                   int id,
                                                   esp_err_t err,
                                                   const cJSON *result,
                                                   const cJSON *error,
                                                   void *user_ctx);
static void app_ha_registry_query_result_cb(ha_ws_client_t *client,
                                            int id,
                                            esp_err_t err,
                                            const cJSON *result,
                                            const cJSON *error,
                                            void *user_ctx);
static esp_err_t app_ha_registry_query_configured_entities(ha_ws_client_t *client);
static void app_ha_registry_removal_task(void *arg);
static void app_ha_history_ws_result_cb(ha_ws_client_t *client,
                                        int id,
                                        esp_err_t err,
                                        const cJSON *result,
                                        const cJSON *error,
                                        void *user_ctx);
static void app_ha_history_delivery_task(void *arg);

typedef struct {
    app_ha_history_result_cb_t callback;
    void *user_ctx;
} app_ha_history_request_t;

typedef struct {
    app_ha_history_request_t *request;
    int request_id;
    esp_err_t result;
    cJSON *history;
    cJSON *error;
} app_ha_history_delivery_t;


/**
 * @brief Handles various device events including key input, configuration updates, and product test mode actions.
 *
 * This event handler responds to:
 * - KEY_EVENT: Handles single-click (toggle screen), double-click (forget password), and long press (factory
 * reset).
 * - CONFIG_EVENT: Handles configuration file upload and status reporting to the web server.
 * - PRODUCT_TEST_EVENT: Handles Wi-Fi connection in product test mode and performs Zigbee signal verification.
 *
 * @param arg          Pointer to user-defined argument (usually unused).
 * @param event_base   Event base identifier (e.g., KEY_EVENT, CONFIG_EVENT).
 * @param event_id     Specific event ID within the event base (e.g., BUTTON_SINGLE_CLICK).
 * @param event_data   Pointer to event-specific data (may be NULL).
 */
static void device_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == FIRMWARE_UPDATE_EVENT) {
        if (event_id == FIRMWARE_UPDATE_EVENT_STATUS_CHANGED) {
            const firmware_update_status_t *status = event_data;
            if (status) {
                bool tls_operation_active =
                    status->state == FIRMWARE_UPDATE_STATE_CHECKING ||
                    status->state == FIRMWARE_UPDATE_STATE_DOWNLOADING ||
                    status->state == FIRMWARE_UPDATE_STATE_VERIFYING;
                mdns_scan_set_paused(tls_operation_active);
                app_reconcile_ha_data_for_firmware_status(status);
            }
            app_reconcile_firmware_ui();
        }
    } else if (event_base == SCREEN_EVENT) {
        switch (event_id) {
        case SCREEN_EVENT_CONFIG_CHANGED:
            ESP_LOGI(TAG, "SCREEN_EVENT_CONFIG_CHANGED");
            if (lvgl_port_lock(0)) {
                esp_err_t ret = screen_ui_reload();
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to reload screen UI: %s", esp_err_to_name(ret));
                }
                lvgl_port_unlock();
            }
            app_screen_state_reconcile();
            taskENTER_CRITICAL(&s_ha_data_state_guard);
            ha_ws_client_t *registry_client =
                s_ha_registry_subscription_client;
            taskEXIT_CRITICAL(&s_ha_data_state_guard);
            if (registry_client) {
                esp_err_t query_err =
                    app_ha_registry_query_configured_entities(registry_client);
                if (query_err != ESP_OK) {
                    ESP_LOGW(TAG,
                             "Failed to refresh HA entity options after screen config change: %s",
                             esp_err_to_name(query_err));
                }
            }
            break;
        default:
            ESP_LOGE(TAG, "not support screen evt");
            break;
        }
    } else if (event_base == KEY_EVENT) {
        switch (event_id) {
        case BUTTON_SINGLE_CLICK: {
            ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
            firmware_update_status_t firmware_status = {0};
            if (firmware_update_get_status(&firmware_status) == ESP_OK &&
                firmware_status.state != FIRMWARE_UPDATE_STATE_CHECKING &&
                firmware_update_is_busy()) {
                ESP_LOGI(TAG, "Ignoring BOOT key while OTA update is in progress");
                break;
            }
            if (webserver_config_forget_password_get_state() !=
                WEBSERVER_FORGOT_PASSWORD_STATE_DONE) {
                ESP_LOGI(TAG, "Ignoring BOOT key while password recovery is active");
                break;
            }
            if (!s_screen_power_ready) {
                ESP_LOGW(TAG, "Ignoring BOOT key before screen initialization");
                break;
            }
            esp_err_t err = screen_toggle_power();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "BOOT key screen toggle failed: %s", esp_err_to_name(err));
            }
        }
        break;
        case BUTTON_DOUBLE_CLICK: {
            ESP_LOGI(TAG, "BUTTON_DOUBLE_CLICK");
            webserver_config_forget_password_double_click();
        }
        break;
        case BUTTON_LONG_PRESS_START: {
            ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START");
            // rgb_set_indicator_effect(RGB_RESTORE_FACTORY_SETTING);
            BaseType_t created = xTaskCreate(app_factory_reset_task,
                                             "factory_reset",
                                             4096,
                                             NULL,
                                             5,
                                             NULL);
            if (created != pdPASS) {
                ESP_LOGE(TAG, "Failed to create factory reset task");
                webserver_config_restore_factory_settings();
            }
        }
        break;

        default: {
            ESP_LOGE(TAG, "not support evt");
        }
        break;
        }
    } else if (event_base == WIFI_EVENT || event_base == IP_EVENT) {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
            firmware_auto_check_notify_wifi_connected();
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            firmware_auto_check_notify_wifi_disconnected();
            NetworkState *network = network_get_state();
            bool wifi_configured = network && network->wifi_params.ssid[0] != '\0';
            bool has_ethernet = network && network->eth_connected;
            app_set_wifi_connected_toast_pending(false);
            app_set_wifi_status_connected(false);
            app_set_notification_active(SCREEN_NOTIFICATION_WIFI_DISCONNECTED,
                                        wifi_configured && !has_ethernet);
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            s_ha_network_ip_ready = true;
            app_set_wifi_status_connected(true);
            app_set_notification_active(SCREEN_NOTIFICATION_WIFI_DISCONNECTED, false);
            app_set_wifi_connected_toast_pending(
                app_screen_state_resolve_target() == SCREEN_PAGE_HA_PROVISIONING);
            /* Both services were started at boot, before this interface had an
             * address: HA failed the handshake outright, and SNTP is still
             * waiting out lwIP's retry timer. Nudge both now. */
            sys_time_on_network_ready();
            firmware_auto_check_notify_wifi_got_ip();
            app_ha_data_post_request(APP_HA_DATA_REQUEST_RECONCILE);
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
            app_set_wifi_status_connected(false);
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
            s_ha_network_ip_ready = true;
            app_set_notification_active(SCREEN_NOTIFICATION_WIFI_DISCONNECTED, false);
            sys_time_on_network_ready();
            app_ha_data_post_request(APP_HA_DATA_REQUEST_RECONCILE);
        }
        app_screen_state_reconcile();
    }
}

/**
 * @brief Registers the device event handler for key and screen events.
 *
 * This function binds the global event handler `device_event_handler` to handle:
 * - KEY_EVENT
 * - SCREEN_EVENT
 *
 * @return ESP_OK on success.
 */
static uint32_t device_event_init(void)
{
    esp_event_handler_register(KEY_EVENT, ESP_EVENT_ANY_ID, &device_event_handler, NULL);
    esp_event_handler_register(SCREEN_EVENT, ESP_EVENT_ANY_ID, &device_event_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &device_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &device_event_handler, NULL);
    esp_event_handler_register(FIRMWARE_UPDATE_EVENT,
                               FIRMWARE_UPDATE_EVENT_STATUS_CHANGED,
                               &device_event_handler,
                               NULL);
    // esp_event_handler_register(PRODUCT_TEST_EVENT, ESP_EVENT_ANY_ID, &device_event_handler, NULL);
    // esp_event_handler_register(CONFIG_EVENT, ESP_EVENT_ANY_ID, &device_event_handler, NULL);
    return ESP_OK;
}

static void screen_lock_reset_timer_cb(lv_event_t *e)
{
    screen_lock_reset_timer();
}

static lv_indev_read_cb_t s_touch_read_cb;
static bool s_suppress_wake_touch;

static void app_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    s_touch_read_cb(indev, data);

    if (data->state == LV_INDEV_STATE_PRESSED && screen_is_locked()) {
        s_suppress_wake_touch = true;
        screen_lock_reset_timer();
    }

    if (s_suppress_wake_touch) {
        if (data->state == LV_INDEV_STATE_RELEASED) {
            s_suppress_wake_touch = false;
        }
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void screen_press_reset_timer(void)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev != NULL) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            s_touch_read_cb = lv_indev_get_read_cb(indev);
            if (s_touch_read_cb) {
                lv_indev_set_read_cb(indev, app_touch_read_cb);
            }
            lv_indev_add_event_cb(indev, screen_lock_reset_timer_cb, LV_EVENT_PRESSED, NULL);
            break;
        }
        indev = lv_indev_get_next(indev);
    }
}

ha_entity_store_t *app_get_ha_entity_store(void)
{
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    ha_entity_store_t *store = s_ha_entity_store;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    return store;
}

/**
 * @brief Queue a request for the HA entity data manager.
 *
 * A start request is dropped while no interface has an IP yet. The very first
 * request used to be issued from app_ha_data_manager_init(), which runs long
 * before DHCP completes, so the WebSocket handshake failed with "Host is
 * unreachable" and the UI briefly reported Home Assistant as disconnected.
 * That request is re-issued from the IP_EVENT handlers once an address is
 * available.
 *
 * Reconciles for an already running data layer are always queued, since those
 * only follow user edits. Stop requests are never deferred.
 */
static void app_ha_data_post_request(app_ha_data_request_t request)
{
    if (!s_ha_data_reconcile_queue) {
        return;
    }
    if (request == APP_HA_DATA_REQUEST_RECONCILE &&
        !s_ha_network_ip_ready && !app_get_ha_entity_store()) {
        ESP_LOGD(TAG, "Deferring HA entity data start until the network has an IP");
        return;
    }
    xQueueOverwrite(s_ha_data_reconcile_queue, &request);
}

static uint32_t app_ha_data_get_start_delay_ms(void)
{
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    int64_t not_before_us = s_ha_data_start_not_before_us;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);

    int64_t remaining_us = not_before_us - esp_timer_get_time();
    if (remaining_us <= 0) {
        return 0;
    }
    return (uint32_t)((remaining_us + 999) / 1000);
}

static bool app_finish_ha_ota_resume(void)
{
    bool changed = false;
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    if (s_ha_ota_connection_state == APP_HA_OTA_CONNECTION_RESUMING) {
        s_ha_ota_connection_state = APP_HA_OTA_CONNECTION_NORMAL;
        changed = true;
    }
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    return changed;
}

esp_err_t app_ha_call_entity_service(const char *domain,
                                     const char *service,
                                     const char *entity_id)
{
    return app_ha_call_entity_service_data(domain,
                                           service,
                                           entity_id,
                                           NULL);
}

esp_err_t app_ha_call_entity_service_data(const char *domain,
                                          const char *service,
                                          const char *entity_id,
                                          const cJSON *service_data)
{
    if (!domain || !domain[0] || !service || !service[0] ||
        !entity_id || !entity_id[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    taskENTER_CRITICAL(&s_ha_data_state_guard);
    ha_ws_client_t *client = s_ha_ws_client;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    if (!client) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *target = cJSON_CreateObject();
    if (!target) {
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(target, "entity_id", entity_id)) {
        cJSON_Delete(target);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ha_ws_call_service(client,
                                       domain,
                                       service,
                                       service_data,
                                       target,
                                       false,
                                       NULL,
                                       NULL,
                                       NULL);
    cJSON_Delete(target);
    return err;
}

esp_err_t app_ha_get_entity_history(const char *entity_id,
                                    time_t start_time,
                                    time_t end_time,
                                    app_ha_history_result_cb_t callback,
                                    void *user_ctx,
                                    int *request_id_out)
{
    if (!entity_id || !entity_id[0] || !callback ||
        start_time < 0 || end_time <= start_time) {
        return ESP_ERR_INVALID_ARG;
    }

    taskENTER_CRITICAL(&s_ha_data_state_guard);
    ha_ws_client_t *client = s_ha_ws_client;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    if (!client) {
        return ESP_ERR_INVALID_STATE;
    }

    char start_iso[40] = {0};
    char end_iso[40] = {0};
    if (sys_time_format_iso8601_time_by_utc_ms((uint64_t)start_time * 1000,
                                               start_iso,
                                               sizeof(start_iso)) != 0 ||
        sys_time_format_iso8601_time_by_utc_ms((uint64_t)end_time * 1000,
                                               end_iso,
                                               sizeof(end_iso)) != 0) {
        return ESP_FAIL;
    }

    app_ha_history_request_t *request = calloc(1, sizeof(*request));
    if (!request) {
        return ESP_ERR_NO_MEM;
    }
    request->callback = callback;
    request->user_ctx = user_ctx;

    cJSON *message = cJSON_CreateObject();
    cJSON *entity_ids = cJSON_CreateArray();
    cJSON *entity_id_item = cJSON_CreateString(entity_id);
    if (!message || !entity_ids || !entity_id_item ||
        !cJSON_AddStringToObject(message,
                                "type",
                                "history/history_during_period") ||
        !cJSON_AddStringToObject(message, "start_time", start_iso) ||
        !cJSON_AddStringToObject(message, "end_time", end_iso)) {
        cJSON_Delete(message);
        cJSON_Delete(entity_ids);
        cJSON_Delete(entity_id_item);
        free(request);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToArray(entity_ids, entity_id_item);
    cJSON_AddItemToObject(message, "entity_ids", entity_ids);
    cJSON_AddBoolToObject(message, "include_start_time_state", false);
    cJSON_AddBoolToObject(message, "significant_changes_only", false);
    cJSON_AddBoolToObject(message, "minimal_response", true);
    cJSON_AddBoolToObject(message, "no_attributes", true);

    esp_err_t err = ha_ws_send_command(client,
                                       message,
                                       app_ha_history_ws_result_cb,
                                       request,
                                       request_id_out);
    cJSON_Delete(message);
    if (err != ESP_OK) {
        free(request);
    }
    return err;
}

static void app_ha_history_ws_result_cb(ha_ws_client_t *client,
                                        int id,
                                        esp_err_t err,
                                        const cJSON *result,
                                        const cJSON *error,
                                        void *user_ctx)
{
    (void)client;
    app_ha_history_request_t *request = user_ctx;
    if (!request) {
        return;
    }

    app_ha_history_delivery_t *delivery = calloc(1, sizeof(*delivery));
    if (!delivery) {
        free(request);
        return;
    }
    delivery->request = request;
    delivery->request_id = id;
    delivery->result = err;
    delivery->history = result ? cJSON_Duplicate(result, true) : NULL;
    delivery->error = error ? cJSON_Duplicate(error, true) : NULL;
    if ((result && !delivery->history) || (error && !delivery->error)) {
        cJSON_Delete(delivery->history);
        cJSON_Delete(delivery->error);
        free(request);
        free(delivery);
        return;
    }

    BaseType_t created = xTaskCreate(app_ha_history_delivery_task,
                                     "ha_history_result",
                                     6144,
                                     delivery,
                                     APP_HA_DATA_TASK_PRIORITY,
                                     NULL);
    if (created != pdPASS) {
        cJSON_Delete(delivery->history);
        cJSON_Delete(delivery->error);
        free(request);
        free(delivery);
    }
}

static void app_ha_history_delivery_task(void *arg)
{
    app_ha_history_delivery_t *delivery = arg;
    if (delivery && delivery->request && delivery->request->callback) {
        delivery->request->callback(delivery->request_id,
                                    delivery->result,
                                    delivery->history,
                                    delivery->error,
                                    delivery->request->user_ctx);
    }
    if (delivery) {
        cJSON_Delete(delivery->history);
        cJSON_Delete(delivery->error);
        free(delivery->request);
        free(delivery);
    }
    vTaskDelete(NULL);
}

static void app_ha_connection_cb(ha_ws_client_t *client,
                                 ha_ws_event_t event,
                                 void *user_ctx)
{
    (void)user_ctx;

    bool ota_resume_finished = false;
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    bool is_current = client == s_ha_ws_client;
    if (is_current) {
        switch (event) {
        case HA_WS_EVENT_CONNECTED:
        case HA_WS_EVENT_AUTH_REQUIRED:
        case HA_WS_EVENT_AUTHENTICATED:
        case HA_WS_EVENT_AUTH_INVALID:
            s_ha_transport_connected = true;
            break;
        case HA_WS_EVENT_DISCONNECTED:
        case HA_WS_EVENT_ERROR:
            s_ha_transport_connected = false;
            break;
        default:
            break;
        }
        if (s_ha_ota_connection_state == APP_HA_OTA_CONNECTION_RESUMING &&
            (event == HA_WS_EVENT_AUTHENTICATED ||
             event == HA_WS_EVENT_AUTH_INVALID ||
             event == HA_WS_EVENT_DISCONNECTED ||
             event == HA_WS_EVENT_ERROR)) {
            s_ha_ota_connection_state = APP_HA_OTA_CONNECTION_NORMAL;
            ota_resume_finished = true;
        }
    }
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    if (!is_current) {
        return;
    }

    if (ota_resume_finished) {
        ESP_LOGI(TAG,
                 "HA data connection OTA resume finished: event=%d",
                 (int)event);
    }

    switch (event) {
    case HA_WS_EVENT_AUTHENTICATED: {
        ESP_LOGI(TAG, "HA data connection authenticated, version=%s", ha_ws_get_ha_version(client));
        ha_screen_light_start(client);
        taskENTER_CRITICAL(&s_ha_data_state_guard);
        bool subscribe_registry = s_ha_registry_subscription_client != client;
        if (subscribe_registry) {
            s_ha_registry_subscription_client = client;
        }
        taskEXIT_CRITICAL(&s_ha_data_state_guard);
        if (subscribe_registry) {
            esp_err_t subscribe_err = ha_ws_subscribe_events(
                client,
                "entity_registry_updated",
                app_ha_registry_event_cb,
                app_ha_registry_subscription_result_cb,
                NULL,
                NULL);
            if (subscribe_err != ESP_OK) {
                ESP_LOGE(TAG,
                         "Failed to subscribe to HA entity registry updates: %s",
                         esp_err_to_name(subscribe_err));
                taskENTER_CRITICAL(&s_ha_data_state_guard);
                if (s_ha_registry_subscription_client == client) {
                    s_ha_registry_subscription_client = NULL;
                }
                taskEXIT_CRITICAL(&s_ha_data_state_guard);
            }
        }
        {
            esp_err_t query_err = app_ha_registry_query_configured_entities(client);
            if (query_err != ESP_OK) {
                ESP_LOGE(TAG,
                         "Failed to query configured HA entity registry entries: %s",
                         esp_err_to_name(query_err));
            }
        }
        break;
    }
    case HA_WS_EVENT_AUTH_INVALID:
        ESP_LOGE(TAG, "HA data connection rejected the access token");
        /* Stop from the manager task rather than from the WebSocket callback.
         * esp_websocket_client_stop() must not run on its own event task. */
        app_ha_data_post_request(APP_HA_DATA_REQUEST_STOP);
        break;
    case HA_WS_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "HA data connection disconnected");
        break;
    case HA_WS_EVENT_ERROR:
        ESP_LOGW(TAG, "HA data connection error");
        break;
    default:
        break;
    }
    ha_auth_handle_ws_event(event);
}

static bool app_ha_registry_client_is_current(ha_ws_client_t *client)
{
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    bool current = client && client == s_ha_ws_client;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    return current;
}

static void app_ha_registry_free_removal_job(app_ha_registry_removal_job_t *job)
{
    if (!job) {
        return;
    }
    screen_free_entity_ids(job->entity_ids, job->entity_id_count);
    free(job);
}

static bool app_ha_registry_job_contains(
    const app_ha_registry_removal_job_t *job,
    const char *entity_id)
{
    if (!job || !entity_id) {
        return false;
    }
    for (size_t i = 0; i < job->entity_id_count; i++) {
        if (job->entity_ids[i] && strcmp(job->entity_ids[i], entity_id) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t app_ha_registry_merge_removal_job(
    app_ha_registry_removal_job_t *destination,
    app_ha_registry_removal_job_t *source)
{
    if (!destination || !source) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t capacity = destination->entity_id_count + source->entity_id_count;
    char **resized = realloc(destination->entity_ids,
                             capacity * sizeof(*destination->entity_ids));
    if (!resized) {
        return ESP_ERR_NO_MEM;
    }
    destination->entity_ids = resized;

    for (size_t i = 0; i < source->entity_id_count; i++) {
        char *entity_id = source->entity_ids[i];
        if (!entity_id) {
            continue;
        }
        if (app_ha_registry_job_contains(destination, entity_id)) {
            free(entity_id);
        } else {
            destination->entity_ids[destination->entity_id_count++] = entity_id;
        }
        source->entity_ids[i] = NULL;
    }
    free(source->entity_ids);
    free(source);
    return ESP_OK;
}

static void app_ha_registry_enqueue_removal_job(app_ha_registry_removal_job_t *job)
{
    if (!job || job->entity_id_count == 0) {
        app_ha_registry_free_removal_job(job);
        return;
    }
    if (!s_ha_registry_pending_lock || !s_ha_registry_removal_queue) {
        ESP_LOGE(TAG, "HA entity registry removal worker is not initialized");
        app_ha_registry_free_removal_job(job);
        return;
    }

    job->next = NULL;
    xSemaphoreTake(s_ha_registry_pending_lock, portMAX_DELAY);
    if (s_ha_registry_pending_tail) {
        s_ha_registry_pending_tail->next = job;
    } else {
        s_ha_registry_pending_head = job;
    }
    s_ha_registry_pending_tail = job;
    xSemaphoreGive(s_ha_registry_pending_lock);

    uint8_t signal = 1;
    if (xQueueOverwrite(s_ha_registry_removal_queue, &signal) != pdPASS) {
        ESP_LOGE(TAG, "Failed to wake HA entity registry removal worker");
    }
}

static app_ha_registry_removal_job_t *app_ha_registry_take_pending_jobs(void)
{
    xSemaphoreTake(s_ha_registry_pending_lock, portMAX_DELAY);
    app_ha_registry_removal_job_t *jobs = s_ha_registry_pending_head;
    s_ha_registry_pending_head = NULL;
    s_ha_registry_pending_tail = NULL;
    xSemaphoreGive(s_ha_registry_pending_lock);
    return jobs;
}

static app_ha_registry_removal_job_t *app_ha_registry_create_single_removal_job(
    const char *entity_id)
{
    if (!entity_id || !entity_id[0]) {
        return NULL;
    }
    app_ha_registry_removal_job_t *job = calloc(1, sizeof(*job));
    if (!job) {
        return NULL;
    }
    job->entity_ids = calloc(1, sizeof(*job->entity_ids));
    if (!job->entity_ids) {
        free(job);
        return NULL;
    }
    job->entity_ids[0] = strdup(entity_id);
    if (!job->entity_ids[0]) {
        app_ha_registry_free_removal_job(job);
        return NULL;
    }
    job->entity_id_count = 1;
    return job;
}

static void app_ha_registry_event_cb(ha_ws_client_t *client,
                                     int subscription_id,
                                     const cJSON *event,
                                     void *user_ctx)
{
    (void)subscription_id;
    (void)user_ctx;
    if (!app_ha_registry_client_is_current(client)) {
        return;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(event, "data");
    const cJSON *action = cJSON_GetObjectItemCaseSensitive(data, "action");
    const cJSON *entity_id = cJSON_GetObjectItemCaseSensitive(data, "entity_id");
    if (!cJSON_IsString(action) || !action->valuestring ||
        !cJSON_IsString(entity_id) || !entity_id->valuestring) {
        return;
    }

    if (strcmp(action->valuestring, "remove") != 0) {
        /* The event only identifies the changed entity. Re-query configured
         * entries so favorite_colors changes become visible on the screen. */
        esp_err_t err = app_ha_registry_query_configured_entities(client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "Failed to refresh HA entity options after registry update: %s",
                     esp_err_to_name(err));
        }
        return;
    }

    /* Registry updates also cover renames and disabled-state changes. Keep
     * those references; only an actual registry removal invalidates a tile. */
    light_favorite_colors_store_remove(entity_id->valuestring);

    app_ha_registry_removal_job_t *job =
        app_ha_registry_create_single_removal_job(entity_id->valuestring);
    if (!job) {
        ESP_LOGE(TAG,
                 "Failed to queue removed HA entity %s",
                 entity_id->valuestring);
        return;
    }
    app_ha_registry_enqueue_removal_job(job);
}

static void app_ha_registry_subscription_result_cb(ha_ws_client_t *client,
                                                   int id,
                                                   esp_err_t err,
                                                   const cJSON *result,
                                                   const cJSON *error,
                                                   void *user_ctx)
{
    (void)id;
    (void)result;
    (void)error;
    (void)user_ctx;
    if (!app_ha_registry_client_is_current(client)) {
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "HA entity registry subscription ended with an error: %s",
                 esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Subscribed to HA entity registry updates");
    }
}

static void app_ha_registry_query_result_cb(ha_ws_client_t *client,
                                            int id,
                                            esp_err_t err,
                                            const cJSON *result,
                                            const cJSON *error,
                                            void *user_ctx)
{
    (void)id;
    (void)error;
    (void)user_ctx;
    if (!app_ha_registry_client_is_current(client)) {
        return;
    }
    if (err != ESP_OK || !cJSON_IsObject(result)) {
        ESP_LOGE(TAG,
                 "Failed to read HA entity registry entries: %s",
                 esp_err_to_name(err != ESP_OK ? err : HA_WS_ERR_INVALID_RESPONSE));
        return;
    }

    size_t missing_count = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, result) {
        if (entry->string && cJSON_IsObject(entry) &&
            strncmp(entry->string, "light.", 6) == 0) {
            bool changed = false;
            esp_err_t options_err = light_favorite_colors_store_update(
                entry->string,
                entry,
                &changed);
            if (options_err != ESP_OK) {
                ESP_LOGW(TAG,
                         "Failed to cache favorite colors for %s: %s",
                         entry->string,
                         esp_err_to_name(options_err));
            }
            else if (changed && lvgl_port_lock(0)) {
                taskENTER_CRITICAL(&s_ha_data_state_guard);
                ha_entity_store_t *store = s_ha_entity_store;
                taskEXIT_CRITICAL(&s_ha_data_state_guard);
                if (store) {
                    screen_ui_refresh_entity_all(store, entry->string);
                }
                lvgl_port_unlock();
            }
        }
        /* get_entries returns JSON null for requested IDs which no longer
         * exist. Disabled entities still have a registry entry. */
        if (entry->string && cJSON_IsNull(entry)) {
            light_favorite_colors_store_remove(entry->string);
            missing_count++;
        }
    }
    if (missing_count == 0) {
        return;
    }

    app_ha_registry_removal_job_t *job = calloc(1, sizeof(*job));
    if (!job) {
        ESP_LOGE(TAG, "Failed to allocate HA registry reconciliation job");
        return;
    }
    job->entity_ids = calloc(missing_count, sizeof(*job->entity_ids));
    if (!job->entity_ids) {
        app_ha_registry_free_removal_job(job);
        ESP_LOGE(TAG, "Failed to allocate missing HA entity list");
        return;
    }

    cJSON_ArrayForEach(entry, result) {
        if (!entry->string || !cJSON_IsNull(entry)) {
            continue;
        }
        char *entity_id = strdup(entry->string);
        if (!entity_id) {
            app_ha_registry_free_removal_job(job);
            ESP_LOGE(TAG, "Failed to copy missing HA entity id");
            return;
        }
        job->entity_ids[job->entity_id_count++] = entity_id;
    }
    app_ha_registry_enqueue_removal_job(job);
}

static esp_err_t app_ha_registry_query_configured_entities(ha_ws_client_t *client)
{
    char **entity_ids = NULL;
    size_t entity_id_count = 0;
    esp_err_t err = screen_get_configured_entity_ids(&entity_ids, &entity_id_count);
    if (err != ESP_OK) {
        screen_free_entity_ids(entity_ids, entity_id_count);
        return err;
    }
    light_favorite_colors_store_retain((const char *const *)entity_ids,
                                       entity_id_count);
    if (entity_id_count == 0) {
        screen_free_entity_ids(entity_ids, entity_id_count);
        return ESP_OK;
    }

    cJSON *message = cJSON_CreateObject();
    cJSON *entity_ids_json = cJSON_CreateArray();
    if (!message || !entity_ids_json ||
        !cJSON_AddStringToObject(message,
                                "type",
                                "config/entity_registry/get_entries")) {
        cJSON_Delete(message);
        cJSON_Delete(entity_ids_json);
        screen_free_entity_ids(entity_ids, entity_id_count);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < entity_id_count; i++) {
        cJSON *entity_id = cJSON_CreateString(entity_ids[i]);
        if (!entity_id || !cJSON_AddItemToArray(entity_ids_json, entity_id)) {
            cJSON_Delete(entity_id);
            cJSON_Delete(message);
            cJSON_Delete(entity_ids_json);
            screen_free_entity_ids(entity_ids, entity_id_count);
            return ESP_ERR_NO_MEM;
        }
    }
    if (!cJSON_AddItemToObject(message, "entity_ids", entity_ids_json)) {
        cJSON_Delete(entity_ids_json);
        cJSON_Delete(message);
        screen_free_entity_ids(entity_ids, entity_id_count);
        ESP_LOGE(TAG, "Failed to attach configured entity IDs to registry query");
        return ESP_ERR_NO_MEM;
    }
    err = ha_ws_send_command(client,
                             message,
                             app_ha_registry_query_result_cb,
                             NULL,
                             NULL);
    cJSON_Delete(message);
    screen_free_entity_ids(entity_ids, entity_id_count);
    return err;
}

static void app_ha_registry_process_removal_job(
    app_ha_registry_removal_job_t *job)
{
    if (!job || job->entity_id_count == 0) {
        app_ha_registry_free_removal_job(job);
        return;
    }

    size_t removed = 0;
    esp_err_t err = screen_remove_entity_references(
        (const char *const *)job->entity_ids,
        job->entity_id_count,
        &removed);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to remove deleted HA entities from screen config: %s",
                 esp_err_to_name(err));
    } else if (removed > 0) {
        ESP_LOGI(TAG,
                 "Removed %u reference(s) to %u deleted HA entity/entities from screen config",
                 (unsigned)removed,
                 (unsigned)job->entity_id_count);
        err = screen_config_notify_authenticated_clients();
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "Failed to notify clients after removing deleted HA entities: %s",
                     esp_err_to_name(err));
        }
    }
    app_ha_registry_free_removal_job(job);
}

static void app_ha_registry_removal_task(void *arg)
{
    (void)arg;
    uint8_t signal = 0;
    while (xQueueReceive(s_ha_registry_removal_queue,
                         &signal,
                         portMAX_DELAY) == pdTRUE) {
        app_ha_registry_removal_job_t *pending =
            app_ha_registry_take_pending_jobs();
        app_ha_registry_removal_job_t *batch = NULL;

        while (pending) {
            app_ha_registry_removal_job_t *next = pending->next;
            pending->next = NULL;
            if (!batch) {
                batch = pending;
            } else if (app_ha_registry_merge_removal_job(batch, pending) != ESP_OK) {
                /* Preserve every removal even under allocation pressure. The
                 * current batch is processed before starting a new one. */
                app_ha_registry_process_removal_job(batch);
                batch = pending;
            }
            pending = next;
        }
        app_ha_registry_process_removal_job(batch);
    }
    vTaskDelete(NULL);
}

static void app_ha_entity_changed_cb(ha_entity_store_t *store,
                                     const ha_entity_changed_event_t *event,
                                     void *user_ctx)
{
    (void)user_ctx;

    /* Entity Store callbacks run outside the LVGL task. Serialize access to
     * the current screen before refreshing state_label objects. The view
     * reads both state and domain-specific attributes from the Store cache. */
    if (lvgl_port_lock(0)) {
        screen_ui_refresh_entity_all(store, event->entity_id);
        lvgl_port_unlock();
    }
}

static void app_secure_zero(void *data, size_t size)
{
    volatile unsigned char *cursor = data;
    while (cursor && size--) {
        *cursor++ = 0;
    }
}

static esp_err_t app_ha_data_start(const ha_auth_connection_info_t *info)
{
    if (!info || !info->configured || !info->enabled ||
        !info->hass_url[0] || !info->long_lived_token[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    ha_ws_client_t *ws_client = NULL;
    ha_entity_store_t *entity_store = NULL;
    ha_entity_listener_handle_t *entity_listener = NULL;
    const ha_ws_config_t ws_config = {
        .hass_url = info->hass_url,
        .access_token = info->long_lived_token,
        .auto_reconnect = true,
        .coalesce_messages = true,
        .connection_cb = app_ha_connection_cb,
        .user_ctx = NULL,
    };
    esp_err_t err = ha_ws_client_init(&ws_config, &ws_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HA data WebSocket: %s", esp_err_to_name(err));
        return err;
    }

    const ha_entity_store_config_t store_config = {
        .ws_client = ws_client,
        .max_entities = 64,
        .event_queue_size = 24,
        .worker_stack_size = 6144,
        .worker_priority = 5,
        .prefer_external_memory = true,
    };
    err = ha_entity_store_create(&store_config, &entity_store);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create HA Entity Store: %s", esp_err_to_name(err));
        ha_ws_destroy(ws_client);
        return err;
    }

    err = ha_entity_store_add_listener(entity_store,
                                       app_ha_entity_changed_cb,
                                       NULL,
                                       &entity_listener);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register HA entity listener: %s", esp_err_to_name(err));
        ha_entity_store_destroy(entity_store);
        ha_ws_destroy(ws_client);
        return err;
    }

    err = ha_entity_store_start(entity_store);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HA Entity Store: %s", esp_err_to_name(err));
        ha_entity_store_remove_listener(entity_listener);
        ha_entity_store_destroy(entity_store);
        ha_ws_destroy(ws_client);
        return err;
    }

    if (!lvgl_port_lock(0)) {
        ha_entity_store_stop(entity_store);
        ha_entity_store_remove_listener(entity_listener);
        ha_entity_store_destroy(entity_store);
        ha_ws_close(ws_client);
        ha_ws_destroy(ws_client);
        return ESP_ERR_TIMEOUT;
    }
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    s_ha_ws_client = ws_client;
    s_ha_entity_store = entity_store;
    s_ha_transport_connected = false;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    s_ha_entity_listener = entity_listener;
    strlcpy(s_ha_data_hass_url, info->hass_url, sizeof(s_ha_data_hass_url));
    strlcpy(s_ha_data_token, info->long_lived_token, sizeof(s_ha_data_token));
    screen_ui_set_entity_store(entity_store);
    lvgl_port_unlock();

    ESP_LOGI(TAG,
             "HA WebSocket heap before start: internal_free=%u largest_internal=%u task_stack=%d",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             CONFIG_HA_WS_TASK_STACK_SIZE);
    err = ha_ws_connect(ws_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect HA data WebSocket: %s", esp_err_to_name(err));
        (void)app_ha_data_stop();
        return err;
    }

    ESP_LOGI(TAG, "HA entity data layer initialized for %s", info->hass_url);
    return ESP_OK;
}

static bool app_ha_data_stop(void)
{
    if (!ha_screen_light_stop()) {
        return false;
    }
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    ha_ws_client_t *ws_client = s_ha_ws_client ? s_ha_ws_client : s_ha_ws_cleanup_pending;
    ha_entity_store_t *entity_store = s_ha_entity_store
                                          ? s_ha_entity_store
                                          : s_ha_entity_store_cleanup_pending;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    ha_entity_listener_handle_t *entity_listener = s_ha_entity_listener;
    if (!ws_client && !entity_store) {
        light_favorite_colors_store_clear();
        return true;
    }

    if (!lvgl_port_lock(1000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL while stopping HA entity data layer");
        return false;
    }
    light_favorite_colors_store_clear();
    screen_ui_set_entity_store(NULL);
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    s_ha_ws_client = NULL;
    s_ha_entity_store = NULL;
    s_ha_registry_subscription_client = NULL;
    s_ha_transport_connected = false;
    s_ha_ws_cleanup_pending = ws_client;
    s_ha_entity_store_cleanup_pending = entity_store;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    s_ha_entity_listener = NULL;
    lvgl_port_unlock();

    ha_entity_store_remove_listener(entity_listener);
    if (entity_store) {
        esp_err_t err = ha_entity_store_stop(entity_store);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop HA Entity Store: %s", esp_err_to_name(err));
        }
        err = ha_entity_store_destroy(entity_store);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to destroy HA Entity Store: %s", esp_err_to_name(err));
            return false;
        }
        taskENTER_CRITICAL(&s_ha_data_state_guard);
        if (s_ha_entity_store_cleanup_pending == entity_store) {
            s_ha_entity_store_cleanup_pending = NULL;
        }
        taskEXIT_CRITICAL(&s_ha_data_state_guard);
    }
    if (ws_client) {
        esp_err_t close_err = ha_ws_close(ws_client);
        if (close_err != ESP_OK && close_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Failed to close HA WebSocket before destroy: %s",
                     esp_err_to_name(close_err));
        }
        esp_err_t destroy_err = ha_ws_destroy(ws_client);
        if (destroy_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to destroy HA WebSocket: %s",
                     esp_err_to_name(destroy_err));
            return false;
        }
        taskENTER_CRITICAL(&s_ha_data_state_guard);
        if (s_ha_ws_cleanup_pending == ws_client) {
            s_ha_ws_cleanup_pending = NULL;
        }
        taskEXIT_CRITICAL(&s_ha_data_state_guard);
    }
    app_secure_zero(s_ha_data_hass_url, sizeof(s_ha_data_hass_url));
    app_secure_zero(s_ha_data_token, sizeof(s_ha_data_token));
    return true;
}

static esp_err_t app_ha_data_reconcile(void)
{
    ha_auth_connection_info_t info = {0};
    esp_err_t err = ha_auth_get_connection_info(&info);
    if (err != ESP_OK) {
        return err;
    }

    bool should_run = info.configured && info.enabled &&
                      info.reason != HA_AUTH_REASON_AUTH_INVALID &&
                      info.hass_url[0] && info.long_lived_token[0];
    bool unchanged = should_run && app_get_ha_entity_store() &&
                     strcmp(s_ha_data_hass_url, info.hass_url) == 0 &&
                     strcmp(s_ha_data_token, info.long_lived_token) == 0;

    if (!unchanged) {
        if (!app_ha_data_stop()) {
            app_secure_zero(info.long_lived_token, sizeof(info.long_lived_token));
            return ESP_ERR_TIMEOUT;
        }
        if (should_run) {
            err = app_ha_data_start(&info);
        } else {
            ESP_LOGI(TAG, "HA entity data layer is disabled or not configured");
            err = ESP_OK;
            if (app_finish_ha_ota_resume()) {
                ESP_LOGI(TAG, "HA OTA suspension cleared without reconnecting");
                app_reconcile_ha_notifications();
            }
        }
    }
    else if (app_finish_ha_ota_resume()) {
        ESP_LOGI(TAG, "HA data connection remained active after OTA transfer");
        app_reconcile_ha_notifications();
    }

    app_secure_zero(info.long_lived_token, sizeof(info.long_lived_token));
    return err;
}

static void app_ha_auth_event_handler(void *arg,
                                      esp_event_base_t event_base,
                                      int32_t event_id,
                                      void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;
    if (event_id == HA_AUTH_EVENT_LONG_LIVED_TOKEN_CREATED) {
        esp_err_t err = screen_reset_config();
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "Failed to reset screen config after HA token creation: %s",
                     esp_err_to_name(err));
        }
        else {
            err = screen_config_notify_authenticated_clients();
            if (err != ESP_OK) {
                ESP_LOGW(TAG,
                         "Failed to notify authenticated clients about screen config reset: %s",
                         esp_err_to_name(err));
            }
        }
        int64_t not_before_us = esp_timer_get_time() +
                                (int64_t)APP_HA_DATA_AUTH_SETTLE_MS * 1000;
        taskENTER_CRITICAL(&s_ha_data_state_guard);
        s_ha_data_start_not_before_us = not_before_us;
        taskEXIT_CRITICAL(&s_ha_data_state_guard);
        return;
    }
    if (event_id != HA_AUTH_EVENT_CONNECTION_INFO_CHANGED &&
        event_id != HA_AUTH_EVENT_STATUS_CHANGED &&
        event_id != HA_AUTH_EVENT_CONNECTION_SUSPEND_REQUESTED) {
        return;
    }

    if (s_ha_data_reconcile_queue && event_id != HA_AUTH_EVENT_STATUS_CHANGED) {
        app_ha_data_post_request(event_id == HA_AUTH_EVENT_CONNECTION_SUSPEND_REQUESTED
                                     ? APP_HA_DATA_REQUEST_STOP
                                     : APP_HA_DATA_REQUEST_RECONCILE);
    }
    if (event_id == HA_AUTH_EVENT_CONNECTION_INFO_CHANGED) {
        app_screen_state_reconcile();
    }
    if (event_id != HA_AUTH_EVENT_CONNECTION_SUSPEND_REQUESTED) {
        app_reconcile_ha_notifications();
    }
}

static void app_factory_reset_task(void *arg)
{
    (void)arg;
    webserver_config_restore_factory_settings();
    vTaskDelete(NULL);
}

static void app_lcd_prepare_black_frame(void)
{
    if (!lvgl_port_lock(0)) {
        ESP_LOGW(TAG, "Failed to lock LVGL while preparing the startup frame");
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_invalidate(screen);
    lv_refr_now(lv_display_get_default());
    lvgl_port_unlock();
}

static void app_lcd_enable_after_first_frame(uint8_t brightness)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(APP_LCD_FIRST_FRAME_TIMEOUT_MS);
    while (screen_state_get_current() == SCREEN_PAGE_NONE &&
           xTaskGetTickCount() - start < timeout) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (screen_state_get_current() == SCREEN_PAGE_NONE) {
        ESP_LOGW(TAG, "Timed out waiting for the initial screen page");
    }

    if (lvgl_port_lock(0)) {
        lv_refr_now(lv_display_get_default());
        lvgl_port_unlock();
    }
    else {
        ESP_LOGW(TAG, "Failed to lock LVGL for the first visible refresh");
    }

    /* The SPI flush finishes asynchronously after LVGL submits the frame. */
    vTaskDelay(pdMS_TO_TICKS(APP_LCD_FLUSH_SETTLE_MS));
    ESP_ERROR_CHECK(bsp_lcd_br_set(brightness));
}

static bool app_set_notification_active(screen_notification_type_t type, bool active)
{
    if (!s_screen_state_ready) {
        return false;
    }

    if (!lvgl_port_lock(0)) {
        ESP_LOGW(TAG, "Failed to lock LVGL while updating notification %d", type);
        return false;
    }
    screen_ui_set_notification_active(type, active);
    lvgl_port_unlock();
    return true;
}

static bool app_set_wifi_connected_toast_pending(bool pending)
{
    if (!s_screen_state_ready) {
        return false;
    }

    if (!lvgl_port_lock(0)) {
        ESP_LOGW(TAG, "Failed to lock LVGL while updating Wi-Fi-connected Toast");
        return false;
    }
    screen_ui_set_wifi_connected_toast_pending(pending);
    lvgl_port_unlock();
    return true;
}

static bool app_set_wifi_status_connected(bool connected)
{
    if (!s_screen_state_ready) {
        return false;
    }

    if (!lvgl_port_lock(0)) {
        ESP_LOGW(TAG, "Failed to lock LVGL while updating Wi-Fi status icon");
        return false;
    }
    screen_ui_set_wifi_sta_connected(connected);
    lvgl_port_unlock();
    return true;
}

static void app_reconcile_ha_data_for_firmware_status(
    const firmware_update_status_t *status)
{
    if (!status) {
        return;
    }

    bool manual_locked;
    taskENTER_CRITICAL(&s_manual_ota_web_guard);
    manual_locked = s_manual_ota_web_lock;
    taskEXIT_CRITICAL(&s_manual_ota_web_guard);

    bool transfer_active = status->state == FIRMWARE_UPDATE_STATE_DOWNLOADING ||
                           status->state == FIRMWARE_UPDATE_STATE_VERIFYING;
    bool manual_install_finishing = manual_locked &&
        (status->state == FIRMWARE_UPDATE_STATE_STAGED ||
         status->state == FIRMWARE_UPDATE_STATE_ACTIVATING ||
         status->state == FIRMWARE_UPDATE_STATE_READY_TO_REBOOT ||
         status->state == FIRMWARE_UPDATE_STATE_REBOOTING);
    bool request_stop = false;
    bool request_resume = false;

    taskENTER_CRITICAL(&s_ha_data_state_guard);
    if (transfer_active) {
        bool ha_data_exists = s_ha_ws_client || s_ha_entity_store;
        if (s_ha_ota_connection_state == APP_HA_OTA_CONNECTION_RESUMING ||
            (s_ha_ota_connection_state == APP_HA_OTA_CONNECTION_NORMAL &&
             ha_data_exists)) {
            s_ha_ota_connection_state = APP_HA_OTA_CONNECTION_SUSPENDED;
            request_stop = true;
        }
    }
    else if (!manual_install_finishing &&
             s_ha_ota_connection_state == APP_HA_OTA_CONNECTION_SUSPENDED) {
        s_ha_ota_connection_state = APP_HA_OTA_CONNECTION_RESUMING;
        request_resume = true;
    }
    taskEXIT_CRITICAL(&s_ha_data_state_guard);

    if (request_stop) {
        ESP_LOGI(TAG,
                 "Suspending HA data connection for firmware %s",
                 firmware_update_state_to_string(status->state));
        app_reconcile_ha_notifications();
        app_ha_data_post_request(APP_HA_DATA_REQUEST_STOP);
    }
    else if (request_resume) {
        ESP_LOGI(TAG,
                 "Resuming HA data connection after firmware %s",
                 firmware_update_state_to_string(status->state));
        app_reconcile_ha_notifications();
        app_ha_data_post_request(APP_HA_DATA_REQUEST_RECONCILE);
    }
}

static void app_reconcile_ha_notifications(void)
{
    ha_auth_connection_info_t info = {0};
    if (ha_auth_get_connection_info(&info) != ESP_OK) {
        return;
    }

    bool ha_connected = info.configured &&
                        info.enabled &&
                        info.device_connected &&
                        info.status == HA_AUTH_STATUS_CONNECTED;
    taskENTER_CRITICAL(&s_ha_data_state_guard);
    bool transport_connected = s_ha_transport_connected;
    bool ota_suppressed =
        s_ha_ota_connection_state != APP_HA_OTA_CONNECTION_NORMAL;
    taskEXIT_CRITICAL(&s_ha_data_state_guard);
    if (s_screen_state_ready && lvgl_port_lock(0)) {
        screen_ui_set_ha_connected(ha_connected && !ota_suppressed);
        lvgl_port_unlock();
    }
    else if (s_screen_state_ready) {
        ESP_LOGW(TAG, "Failed to lock LVGL while updating HA status icon");
    }

    if (!s_notification_ha_state_initialized) {
        s_notification_ha_was_configured = info.configured;
        s_notification_ha_state_initialized = true;
    }
    else {
        bool authorization_removed = s_notification_ha_was_configured && !info.configured;
        if (authorization_removed) {
            app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_CANCELED, true);
        }
        else if (info.configured) {
            app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_CANCELED, false);
        }
        s_notification_ha_was_configured = info.configured;
    }

    if (ota_suppressed) {
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_CANCELED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_DISCONNECTED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_EXPIRED, false);
    }
    else if (!info.configured) {
        app_set_notification_active(SCREEN_NOTIFICATION_HA_DISCONNECTED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_EXPIRED, false);
    }
    else if (!info.enabled || info.status == HA_AUTH_STATUS_DISABLED) {
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_CANCELED, true);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_DISCONNECTED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_EXPIRED, false);
    }
    else if (info.reason == HA_AUTH_REASON_AUTH_INVALID) {
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_CANCELED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_DISCONNECTED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_EXPIRED, true);
    }
    else if (ha_connected || transport_connected) {
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_CANCELED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_DISCONNECTED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_EXPIRED, false);
    }
    else {
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_CANCELED, false);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_DISCONNECTED, true);
        app_set_notification_active(SCREEN_NOTIFICATION_HA_AUTH_EXPIRED, false);
    }

    memset(info.long_lived_token, 0, sizeof(info.long_lived_token));
}

static void app_reconcile_firmware_ui(void)
{
    firmware_update_status_t status = {0};
    if (firmware_update_get_status(&status) != ESP_OK) {
        return;
    }

    app_reconcile_firmware_web_status(&status);

    if (!s_screen_state_ready) {
        return;
    }

    bool update_available = status.state == FIRMWARE_UPDATE_STATE_AVAILABLE ||
                            status.state == FIRMWARE_UPDATE_STATE_STAGED;
    if (lvgl_port_lock(0)) {
        screen_ui_set_ota_update_available(update_available);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGW(TAG, "Failed to lock LVGL while updating OTA status icon");
    }

    firmware_update_staged_info_t staged_info = {0};
    bool staged = status.state == FIRMWARE_UPDATE_STATE_STAGED &&
                  firmware_update_get_staged_info(&staged_info) == ESP_OK &&
                  staged_info.automatic;
    if (staged == s_notification_firmware_staged) {
        return;
    }
    if (app_set_notification_active(SCREEN_NOTIFICATION_FIRMWARE_UPDATE, staged)) {
        s_notification_firmware_staged = staged;
    }
}

static void app_reconcile_firmware_web_status(const firmware_update_status_t *status)
{
    if (!status) {
        return;
    }

    bool manual_locked;
    taskENTER_CRITICAL(&s_manual_ota_web_guard);
    manual_locked = s_manual_ota_web_lock;
    taskEXIT_CRITICAL(&s_manual_ota_web_guard);

    bool automatic_transfer = !manual_locked &&
                              (status->state == FIRMWARE_UPDATE_STATE_DOWNLOADING ||
                               status->state == FIRMWARE_UPDATE_STATE_VERIFYING);
    const char *state = firmware_update_state_to_string(status->state);
    if (manual_locked &&
        (status->state == FIRMWARE_UPDATE_STATE_IDLE ||
         status->state == FIRMWARE_UPDATE_STATE_AVAILABLE ||
         status->state == FIRMWARE_UPDATE_STATE_STAGED ||
         status->state == FIRMWARE_UPDATE_STATE_UP_TO_DATE ||
         status->state == FIRMWARE_UPDATE_STATE_FAILED)) {
        state = "preparing";
    }

    esp_err_t err = webserver_firmware_update_status_set(
        manual_locked || automatic_transfer,
        manual_locked,
        manual_locked ? WEBSERVER_FIRMWARE_UPDATE_SOURCE_SCREEN :
                        (automatic_transfer ? WEBSERVER_FIRMWARE_UPDATE_SOURCE_AUTOMATIC :
                                              WEBSERVER_FIRMWARE_UPDATE_SOURCE_NONE),
        state,
        status->progress,
        status->target_version);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot publish firmware update Web status: %s", esp_err_to_name(err));
    }
}

void app_set_manual_ota_web_lock(bool locked)
{
    taskENTER_CRITICAL(&s_manual_ota_web_guard);
    s_manual_ota_web_lock = locked;
    taskEXIT_CRITICAL(&s_manual_ota_web_guard);

    firmware_update_status_t status = {0};
    if (firmware_update_get_status(&status) == ESP_OK) {
        app_reconcile_ha_data_for_firmware_status(&status);
        app_reconcile_firmware_web_status(&status);
    }
}

static void app_notification_manager_init(void)
{
    app_reconcile_ha_notifications();
    app_reconcile_firmware_ui();

    NetworkState *network = network_get_state();
    bool wifi_sta_ready = network && network->sta_connected_flag &&
                          network->current_ip.ip != 0;
    app_set_wifi_status_connected(wifi_sta_ready);
    bool wifi_configured = network && network->wifi_params.ssid[0] != '\0';
    bool uplink_connected = network && (network->sta_connected_flag || network->eth_connected);
    app_set_notification_active(SCREEN_NOTIFICATION_WIFI_DISCONNECTED,
                                wifi_configured && !uplink_connected);
}

static screen_page_t app_screen_state_resolve_target(void)
{
    screen_page_t password_page = webserver_config_forget_password_get_screen_page();
    if (password_page != SCREEN_PAGE_NONE) {
        return password_page;
    }

    NetworkState *network = network_get_state();
    bool network_configured = network &&
                              (network->eth_connected ||
                               network->sta_connected_flag ||
                               network->wifi_params.ssid[0] != '\0');

    ha_auth_connection_info_t ha_info = {0};
    bool ha_configured = ha_auth_get_connection_info(&ha_info) == ESP_OK && ha_info.configured;
    memset(ha_info.long_lived_token, 0, sizeof(ha_info.long_lived_token));

    screen_page_t target = SCREEN_PAGE_AP_PROVISIONING;
    if (network_configured) {
        if (!ha_configured) {
            target = SCREEN_PAGE_HA_PROVISIONING;
        } else {
            target = SCREEN_PAGE_SPLIT_VIEW;
        }
    }

    return target;
}

static void app_screen_state_reconcile(void)
{
    if (!s_screen_state_ready) {
        return;
    }

    screen_page_t target = app_screen_state_resolve_target();
    esp_err_t err = screen_state_request_transition(target);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to transition screen state: %s", esp_err_to_name(err));
    }
}

static esp_err_t app_screen_state_manager_init(void)
{
    esp_err_t err = screen_ui_init();
    if (err != ESP_OK) {
        return err;
    }

    screen_page_t initial_page = app_screen_state_resolve_target();
    ESP_LOGI(TAG, "Initial screen page resolved to: %s", screen_state_to_string(initial_page));
    err = screen_state_init(initial_page);
    if (err != ESP_OK) {
        return err;
    }

    s_screen_state_ready = true;
    return ESP_OK;
}

static void app_ha_data_task(void *arg)
{
    (void)arg;
    app_ha_data_request_t request = 0;
    bool have_request = false;
    while (true) {
        if (!have_request &&
            xQueueReceive(s_ha_data_reconcile_queue, &request, portMAX_DELAY) != pdTRUE) {
            break;
        }
        have_request = false;

        if (request == APP_HA_DATA_REQUEST_STOP) {
            if (!app_ha_data_stop()) {
                ESP_LOGW(TAG, "Retrying HA data layer stop");
                vTaskDelay(pdMS_TO_TICKS(20));
                app_ha_data_post_request(APP_HA_DATA_REQUEST_STOP);
                continue;
            }
            taskENTER_CRITICAL(&s_ha_data_state_guard);
            bool ota_suspended =
                s_ha_ota_connection_state == APP_HA_OTA_CONNECTION_SUSPENDED;
            taskEXIT_CRITICAL(&s_ha_data_state_guard);
            if (ota_suspended) {
                ESP_LOGI(TAG,
                         "HA data connection suspended for OTA: internal_free=%u "
                         "internal_largest=%u",
                         (unsigned)heap_caps_get_free_size(
                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                         (unsigned)heap_caps_get_largest_free_block(
                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            }
            continue;
        }

        uint32_t settle_delay_ms = app_ha_data_get_start_delay_ms();
        if (settle_delay_ms > 0) {
            ESP_LOGI(TAG,
                     "Deferring HA data connection for %u ms while authorization and UI settle",
                     (unsigned)settle_delay_ms);
            if (xQueueReceive(s_ha_data_reconcile_queue,
                              &request,
                              pdMS_TO_TICKS(settle_delay_ms)) == pdTRUE) {
                have_request = true;
                continue;
            }
        }

        for (size_t attempt = 0;; attempt++) {
            esp_err_t err = app_ha_data_reconcile();
            if (err == ESP_OK) {
                break;
            }

            ESP_LOGE(TAG,
                     "Failed to reconcile HA entity data layer (attempt %u): %s",
                     (unsigned)(attempt + 1),
                     esp_err_to_name(err));
            if (attempt >= sizeof(s_ha_data_retry_delays_ms) /
                               sizeof(s_ha_data_retry_delays_ms[0])) {
                ESP_LOGE(TAG, "HA entity data layer retry limit reached");
                if (app_finish_ha_ota_resume()) {
                    ESP_LOGW(TAG, "HA OTA resume failed; restoring normal notification handling");
                    app_reconcile_ha_notifications();
                }
                break;
            }

            uint32_t retry_delay_ms = s_ha_data_retry_delays_ms[attempt];
            ESP_LOGW(TAG,
                     "Retrying HA entity data connection in %u ms",
                     (unsigned)retry_delay_ms);
            if (xQueueReceive(s_ha_data_reconcile_queue,
                              &request,
                              pdMS_TO_TICKS(retry_delay_ms)) == pdTRUE) {
                have_request = true;
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

static esp_err_t app_ha_data_manager_init(void)
{
    esp_err_t err = light_favorite_colors_store_init();
    if (err != ESP_OK) {
        return err;
    }
    s_ha_data_reconcile_queue = xQueueCreate(1, sizeof(app_ha_data_request_t));
    if (!s_ha_data_reconcile_queue) {
        return ESP_ERR_NO_MEM;
    }
    s_ha_registry_pending_lock = xSemaphoreCreateMutex();
    if (!s_ha_registry_pending_lock) {
        vQueueDelete(s_ha_data_reconcile_queue);
        s_ha_data_reconcile_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_ha_registry_removal_queue = xQueueCreate(
        APP_HA_REGISTRY_SIGNAL_QUEUE_LENGTH,
        sizeof(uint8_t));
    if (!s_ha_registry_removal_queue) {
        vSemaphoreDelete(s_ha_registry_pending_lock);
        s_ha_registry_pending_lock = NULL;
        vQueueDelete(s_ha_data_reconcile_queue);
        s_ha_data_reconcile_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    err = esp_event_handler_register(HA_AUTH_EVENT,
                                     ESP_EVENT_ANY_ID,
                                     app_ha_auth_event_handler,
                                     NULL);
    if (err != ESP_OK) {
        vQueueDelete(s_ha_registry_removal_queue);
        s_ha_registry_removal_queue = NULL;
        vSemaphoreDelete(s_ha_registry_pending_lock);
        s_ha_registry_pending_lock = NULL;
        vQueueDelete(s_ha_data_reconcile_queue);
        s_ha_data_reconcile_queue = NULL;
        return err;
    }

    TaskHandle_t data_task_handle = NULL;
    BaseType_t task_created = xTaskCreate(app_ha_data_task,
                                          "ha_data_manager",
                                          APP_HA_DATA_TASK_STACK_SIZE,
                                          NULL,
                                          APP_HA_DATA_TASK_PRIORITY,
                                          &data_task_handle);
    if (task_created != pdPASS) {
        esp_event_handler_unregister(HA_AUTH_EVENT,
                                     ESP_EVENT_ANY_ID,
                                     app_ha_auth_event_handler);
        vQueueDelete(s_ha_registry_removal_queue);
        s_ha_registry_removal_queue = NULL;
        vSemaphoreDelete(s_ha_registry_pending_lock);
        s_ha_registry_pending_lock = NULL;
        vQueueDelete(s_ha_data_reconcile_queue);
        s_ha_data_reconcile_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    task_created = xTaskCreate(app_ha_registry_removal_task,
                               "ha_registry_sync",
                               APP_HA_REGISTRY_TASK_STACK_SIZE,
                               NULL,
                               APP_HA_REGISTRY_TASK_PRIORITY,
                               NULL);
    if (task_created != pdPASS) {
        vTaskDelete(data_task_handle);
        esp_event_handler_unregister(HA_AUTH_EVENT,
                                     ESP_EVENT_ANY_ID,
                                     app_ha_auth_event_handler);
        vQueueDelete(s_ha_registry_removal_queue);
        s_ha_registry_removal_queue = NULL;
        vSemaphoreDelete(s_ha_registry_pending_lock);
        s_ha_registry_pending_lock = NULL;
        vQueueDelete(s_ha_data_reconcile_queue);
        s_ha_data_reconcile_queue = NULL;
        ESP_LOGE(TAG, "Failed to start HA entity registry reconciliation task");
        return ESP_ERR_NO_MEM;
    }

    /* Covers a soft restart where an address was already obtained before this
     * manager came up; on a cold boot the request is issued from the IP_EVENT
     * handler instead, once DHCP has completed. */
    app_ha_data_post_request(APP_HA_DATA_REQUEST_RECONCILE);
    return ESP_OK;
}

void app_init(void)
{
    device_event_init();

    esp_err_t firmware_update_err = firmware_update_init();
    if (firmware_update_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize firmware update state: %s", esp_err_to_name(firmware_update_err));
    }

    key_init();

    webserver_client_list_init();

    webserver_config_init();
    
    mdns_server_init(WIRELESS_CONTROLLER_WORK_MODE_INVALID, 0);

    network_init();


    sys_time_init();
    night_mode_init();

    esp_err_t auto_check_err = firmware_auto_check_init();
    if (auto_check_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to initialize automatic firmware checks: %s",
                 esp_err_to_name(auto_check_err));
    }

    esp_err_t login_session_err = login_session_init();
    if (login_session_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize remembered login sessions: %s", esp_err_to_name(login_session_err));
    }

    webserver_init();

    ha_auth_api_init();
    esp_err_t ha_auth_err = ha_auth_init();
    if (ha_auth_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Home Assistant authorization: %s", esp_err_to_name(ha_auth_err));
    }

    ESP_LOGI(TAG, "init start");

    ESP_ERROR_CHECK(bsp_lcd_init());
    device_info_t *device_info = webserver_config_get_device_info();
    uint8_t startup_brightness = device_info && device_info->brightness != 0
                                     ? device_info->brightness
                                     : 100;
    app_lcd_prepare_black_frame();

    screen_lock_init();

    screen_press_reset_timer();

    esp_err_t screen_init_err = ESP_ERR_TIMEOUT;
    if (lvgl_port_lock(0)) {
        screen_init_err = screen_ui_reload();
        lvgl_port_unlock();
    }

    esp_err_t screen_state_err = app_screen_state_manager_init();
    if (screen_state_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize screen state manager: %s", esp_err_to_name(screen_state_err));
    }

    app_lcd_enable_after_first_frame(startup_brightness);

    app_notification_manager_init();

    esp_err_t ha_data_err = app_ha_data_manager_init();
    if (ha_data_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HA entity data manager: %s", esp_err_to_name(ha_data_err));
    }

    if (screen_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize screen UI: %s", esp_err_to_name(screen_init_err));
        esp_err_t rollback_err = firmware_update_rollback_and_reboot(screen_init_err);
        if (rollback_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG,
                     "Failed to roll back after screen self-test failure: %s",
                     esp_err_to_name(rollback_err));
        }
        return;
    }

    s_screen_power_ready = true;

    esp_err_t boot_confirm_err = firmware_update_confirm_boot();
    if (boot_confirm_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to confirm pending OTA image: %s",
                 esp_err_to_name(boot_confirm_err));
    }

    ESP_LOGI(TAG, "init end");
}
