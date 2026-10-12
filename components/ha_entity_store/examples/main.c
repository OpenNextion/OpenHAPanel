/*
 * ha_entity_store usage example
 *
 * This example focuses on the Entity Store API. Before app_main() runs, the
 * product application must initialize NVS/networking and connect Wi-Fi.
 *
 * Supply the HA URL and long-lived access token from your own secure
 * configuration. Do not commit a real token to source control.
 */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha_entity_store.h"
#include "ha_ws.h"

#ifndef HA_ENTITY_STORE_EXAMPLE_HA_URL
#define HA_ENTITY_STORE_EXAMPLE_HA_URL "http://homeassistant.local:8123"
#endif

#ifndef HA_ENTITY_STORE_EXAMPLE_HA_TOKEN
#define HA_ENTITY_STORE_EXAMPLE_HA_TOKEN "replace-with-a-long-lived-access-token"
#endif

#define ENTITY_LIGHT "light.living_room"
#define ENTITY_TEMPERATURE "sensor.living_room_temperature"
#define ENTITY_CLIMATE "climate.living_room"

static const char *TAG = "ha_entity_store_example";

static const char *store_status_name(ha_entity_store_status_t status)
{
    switch (status) {
    case HA_ENTITY_STORE_STOPPED:
        return "stopped";
    case HA_ENTITY_STORE_OFFLINE:
        return "offline";
    case HA_ENTITY_STORE_SUBSCRIBING:
        return "subscribing";
    case HA_ENTITY_STORE_SYNCING:
        return "syncing";
    case HA_ENTITY_STORE_READY:
        return "ready";
    case HA_ENTITY_STORE_DEGRADED:
        return "degraded";
    default:
        return "unknown";
    }
}

static void ha_connection_cb(ha_ws_client_t *client,
                             ha_ws_event_t event,
                             void *user_ctx)
{
    (void)user_ctx;

    switch (event) {
    case HA_WS_EVENT_AUTHENTICATED:
        ESP_LOGI(TAG, "HA authenticated, version=%s", ha_ws_get_ha_version(client));
        break;
    case HA_WS_EVENT_AUTH_INVALID:
        ESP_LOGE(TAG, "HA rejected the access token");
        break;
    case HA_WS_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "HA disconnected; cached entities will become stale");
        break;
    case HA_WS_EVENT_ERROR:
        ESP_LOGW(TAG, "HA WebSocket error");
        break;
    default:
        break;
    }
}

/* Store-level events describe synchronization and connection state. */
static void store_event_cb(ha_entity_store_t *store,
                           const ha_entity_store_event_t *event,
                           void *user_ctx)
{
    (void)store;
    (void)user_ctx;

    switch (event->type) {
    case HA_ENTITY_STORE_EVENT_STATUS_CHANGED:
        ESP_LOGI(TAG, "Store status: %s", store_status_name(event->status));
        break;
    case HA_ENTITY_STORE_EVENT_SYNC_STARTED:
        ESP_LOGI(TAG, "Sync %" PRIu32 " started", event->sync_generation);
        break;
    case HA_ENTITY_STORE_EVENT_SYNC_SUCCEEDED:
        ESP_LOGI(TAG,
                 "Sync %" PRIu32 " completed: matched=%u changed=%u",
                 event->sync_generation,
                 (unsigned)event->matched_entities,
                 (unsigned)event->changed_entities);
        break;
    case HA_ENTITY_STORE_EVENT_SYNC_FAILED:
        ESP_LOGW(TAG,
                 "Sync %" PRIu32 " failed: %s",
                 event->sync_generation,
                 esp_err_to_name(event->result));
        break;
    }
}

/* Entity callbacks only identify the changed entity. Read a safe copy using get(). */
static void entity_changed_cb(ha_entity_store_t *store,
                              const ha_entity_changed_event_t *event,
                              void *user_ctx)
{
    (void)user_ctx;

    ha_entity_snapshot_t snapshot;
    esp_err_t err = ha_entity_store_get(store, event->entity_id, &snapshot);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot read %s: %s", event->entity_id, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG,
             "%s state=%s name=%s revision=%" PRIu32 " changes=0x%" PRIx32
             " sync_status=%d",
             snapshot.entity_id,
             snapshot.state,
             snapshot.friendly_name,
             snapshot.revision,
             event->changes,
             snapshot.sync_status);

    /* Dynamic attributes can be read without exposing the internal cJSON. */
    if (strcmp(snapshot.entity_id, ENTITY_LIGHT) == 0) {
        double brightness = 0;
        if (ha_entity_store_get_attr_number(store,
                                            snapshot.entity_id,
                                            "brightness",
                                            &brightness) == ESP_OK) {
            ESP_LOGI(TAG, "Light brightness: %.0f", brightness);
        }
    }
}

void app_main(void)
{
    /* Prerequisite: Wi-Fi is already connected by the product application. */
    if (strcmp(HA_ENTITY_STORE_EXAMPLE_HA_TOKEN,
               "replace-with-a-long-lived-access-token") == 0) {
        ESP_LOGE(TAG, "Configure HA_ENTITY_STORE_EXAMPLE_HA_TOKEN first");
        return;
    }

    esp_err_t err = ESP_OK;
    ha_ws_client_t *ha_client = NULL;
    ha_entity_store_t *store = NULL;
    ha_entity_handle_t *light_handle = NULL;
    ha_entity_handle_t *temperature_handle = NULL;
    ha_entity_handle_t *climate_handle = NULL;
    ha_entity_listener_handle_t *store_listener = NULL;
    ha_entity_listener_handle_t *entity_listener = NULL;

    const ha_ws_config_t ws_config = {
        .hass_url = HA_ENTITY_STORE_EXAMPLE_HA_URL,
        .access_token = HA_ENTITY_STORE_EXAMPLE_HA_TOKEN,
        .auto_reconnect = true,
        .coalesce_messages = false,
        .connection_cb = ha_connection_cb,
        .user_ctx = NULL,
    };
    err = ha_ws_client_init(&ws_config, &ha_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ha_ws_client_init failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    const ha_entity_store_config_t store_config = {
        .ws_client = ha_client,
        .max_entities = 32,
        .event_queue_size = 16,
        .worker_stack_size = 6144,
        .worker_priority = 5,
    };
    err = ha_entity_store_create(&store_config, &store);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ha_entity_store_create failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* Register listeners before start() so startup/sync events are observable. */
    err = ha_entity_store_add_event_listener(store,
                                             store_event_cb,
                                             NULL,
                                             &store_listener);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot add Store listener: %s", esp_err_to_name(err));
        goto cleanup;
    }
    err = ha_entity_store_add_listener(store,
                                       entity_changed_cb,
                                       NULL,
                                       &entity_listener);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot add entity listener: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = ha_entity_store_start(store);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ha_entity_store_start failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* acquire() only registers local interest; sync() rebuilds the subscription. */
    err = ha_entity_store_acquire(store,
                                  ENTITY_LIGHT,
                                  HA_ENTITY_FIELD_ALL,
                                  &light_handle);
    if (err != ESP_OK) {
        goto cleanup;
    }
    err = ha_entity_store_acquire(store,
                                  ENTITY_TEMPERATURE,
                                  HA_ENTITY_FIELD_ALL,
                                  &temperature_handle);
    if (err != ESP_OK) {
        goto cleanup;
    }
    err = ha_entity_store_acquire(store,
                                  ENTITY_CLIMATE,
                                  HA_ENTITY_FIELD_ALL,
                                  &climate_handle);
    if (err != ESP_OK) {
        goto cleanup;
    }

    /*
     * One explicit sync covers all three entities. The Store is currently
     * offline, so this request remains pending until HA authentication.
     */
    err = ha_entity_store_sync(store);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ha_entity_store_sync failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = ha_ws_connect(ha_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ha_ws_connect failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /*
     * From here the Store keeps the entities updated through subscribe_entities.
     * A UI task may call ha_entity_store_get() at any time. To refresh after
     * acquiring more entities, call ha_entity_store_sync(store) again.
     */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        ha_entity_snapshot_t temperature;
        if (ha_entity_store_get(store,
                                ENTITY_TEMPERATURE,
                                &temperature) == ESP_OK) {
            ESP_LOGI(TAG,
                     "Periodic read: %s=%s %s (sync_status=%d)",
                     temperature.entity_id,
                     temperature.state,
                     temperature.unit_of_measurement,
                     temperature.sync_status);
        }
    }

cleanup:
    ESP_LOGE(TAG, "Example initialization failed: %s", esp_err_to_name(err));

    /* Stop first: after it returns no more Store callbacks will be issued. */
    if (store) {
        ha_entity_store_stop(store);
    }

    /* destroy() requires all entity and listener handles to be released. */
    ha_entity_store_release(climate_handle);
    ha_entity_store_release(temperature_handle);
    ha_entity_store_release(light_handle);
    ha_entity_store_remove_listener(entity_listener);
    ha_entity_store_remove_listener(store_listener);

    if (store) {
        ha_entity_store_destroy(store);
    }
    if (ha_client) {
        ha_ws_close(ha_client);
        ha_ws_destroy(ha_client);
    }
}
