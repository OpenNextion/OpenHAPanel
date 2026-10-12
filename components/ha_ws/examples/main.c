#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "ha_ws.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAXIMUM_RETRY 5

static const char *TAG = "ws_demo";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying WiFi connection");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_wifi_event_group, ESP_ERR_NO_MEM, TAG, "Failed to create WiFi event group");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "esp_event_loop_create_default failed");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            &instance_any_id),
                        TAG,
                        "Failed to register WiFi event handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            &instance_got_ip),
                        TAG,
                        "Failed to register IP event handler");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WS_DEMO_WIFI_SSID,
            .password = CONFIG_WS_DEMO_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "esp_wifi_set_mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "esp_wifi_set_config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", CONFIG_WS_DEMO_WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

static void log_ha_error(const cJSON *error)
{
    if (!error) {
        return;
    }
    char *text = cJSON_PrintUnformatted(error);
    if (text) {
        ESP_LOGW(TAG, "HA error: %s", text);
        free(text);
    }
}

static void generic_result_cb(ha_ws_client_t *client,
                              int id,
                              esp_err_t err,
                              const cJSON *result,
                              const cJSON *error,
                              void *user_ctx)
{
    (void)client;
    const char *label = user_ctx;

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s failed, id=%d err=%s", label, id, esp_err_to_name(err));
        log_ha_error(error);
        return;
    }

    if (result && cJSON_IsArray(result)) {
        ESP_LOGI(TAG, "%s ok, id=%d array_size=%d", label, id, cJSON_GetArraySize(result));
    } else {
        ESP_LOGI(TAG, "%s ok, id=%d", label, id);
    }
}

static void state_changed_cb(ha_ws_client_t *client,
                             int subscription_id,
                             const cJSON *event,
                             void *user_ctx)
{
    (void)client;
    (void)user_ctx;

    const cJSON *event_type = cJSON_GetObjectItemCaseSensitive(event, "event_type");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(event, "data");
    const cJSON *entity_id = data ? cJSON_GetObjectItemCaseSensitive(data, "entity_id") : NULL;

    ESP_LOGI(TAG,
             "Event sub=%d type=%s entity=%s",
             subscription_id,
             cJSON_IsString(event_type) ? event_type->valuestring : "?",
             cJSON_IsString(entity_id) ? entity_id->valuestring : "?");
}

static void call_demo_service(ha_ws_client_t *client)
{
    if (strlen(CONFIG_WS_DEMO_SERVICE_ENTITY_ID) == 0) {
        ESP_LOGI(TAG, "Skipping call_service because CONFIG_WS_DEMO_SERVICE_ENTITY_ID is empty");
        return;
    }

    cJSON *target = cJSON_CreateObject();
    if (!target) {
        ESP_LOGE(TAG, "Failed to create service target");
        return;
    }

    cJSON_AddStringToObject(target, "entity_id", CONFIG_WS_DEMO_SERVICE_ENTITY_ID);
    esp_err_t err = ha_ws_call_service(client,
                                       CONFIG_WS_DEMO_SERVICE_DOMAIN,
                                       CONFIG_WS_DEMO_SERVICE_NAME,
                                       NULL,
                                       target,
                                       false,
                                       generic_result_cb,
                                       "call_service",
                                       NULL);
    cJSON_Delete(target);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ha_ws_call_service failed: %s", esp_err_to_name(err));
    }
}

static void ha_connection_cb(ha_ws_client_t *client, ha_ws_event_t event, void *user_ctx)
{
    (void)user_ctx;

    switch (event) {
    case HA_WS_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        break;

    case HA_WS_EVENT_AUTH_REQUIRED:
        ESP_LOGI(TAG, "Home Assistant requested auth");
        break;

    case HA_WS_EVENT_AUTHENTICATED:
        ESP_LOGI(TAG, "Home Assistant authenticated, version=%s", ha_ws_get_ha_version(client));
        ha_ws_ping(client, generic_result_cb, "ping", NULL);
        ha_ws_get_states(client, generic_result_cb, "get_states", NULL);
        ha_ws_get_services(client, generic_result_cb, "get_services", NULL);
        ha_ws_subscribe_events(client,
                               "state_changed",
                               state_changed_cb,
                               generic_result_cb,
                               "subscribe_events",
                               NULL);
        call_demo_service(client);
        break;

    case HA_WS_EVENT_AUTH_INVALID:
        ESP_LOGE(TAG, "Home Assistant rejected the access token");
        break;

    case HA_WS_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket disconnected");
        break;

    case HA_WS_EVENT_ERROR:
        ESP_LOGW(TAG, "WebSocket error");
        break;

    default:
        break;
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (strlen(CONFIG_WS_DEMO_WIFI_SSID) == 0 ||
        strlen(CONFIG_WS_DEMO_WIFI_PASSWORD) == 0 ||
        strlen(CONFIG_WS_DEMO_HA_TOKEN) == 0) {
        ESP_LOGE(TAG, "Configure WiFi and HA token with idf.py menuconfig first");
        return;
    }

    ESP_ERROR_CHECK(wifi_init_sta());

    ha_ws_client_t *ha_client = NULL;
    ha_ws_config_t ha_config = {
        .hass_url = CONFIG_WS_DEMO_HA_URL,
        .access_token = CONFIG_WS_DEMO_HA_TOKEN,
        .auto_reconnect = true,
        .coalesce_messages = false,
        .connection_cb = ha_connection_cb,
    };

    ESP_ERROR_CHECK(ha_ws_client_init(&ha_config, &ha_client));
    ESP_ERROR_CHECK(ha_ws_connect(ha_client));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (ha_ws_is_authenticated(ha_client)) {
            ha_ws_ping(ha_client, generic_result_cb, "periodic_ping", NULL);
        }
    }

}
