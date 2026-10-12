#include "screen.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "littlefs.h"
#include "screen_validate.h"

static const char *TAG = "screen";
static SemaphoreHandle_t s_config_lock;
static portMUX_TYPE s_config_lock_guard = portMUX_INITIALIZER_UNLOCKED;

ESP_EVENT_DEFINE_BASE(SCREEN_EVENT);

static SemaphoreHandle_t screen_get_config_lock(void)
{
    taskENTER_CRITICAL(&s_config_lock_guard);
    SemaphoreHandle_t lock = s_config_lock;
    taskEXIT_CRITICAL(&s_config_lock_guard);
    if (lock) {
        return lock;
    }

    SemaphoreHandle_t created = xSemaphoreCreateRecursiveMutex();
    if (!created) {
        return NULL;
    }

    taskENTER_CRITICAL(&s_config_lock_guard);
    if (!s_config_lock) {
        s_config_lock = created;
        created = NULL;
    }
    lock = s_config_lock;
    taskEXIT_CRITICAL(&s_config_lock_guard);
    if (created) {
        vSemaphoreDelete(created);
    }
    return lock;
}

static esp_err_t screen_save_config_locked(const cJSON *config)
{
    if (screen_validate_config(config) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid screen config");
        return ESP_FAIL;
    }

    cJSON *config_copy = cJSON_Duplicate(config, true);
    if (!config_copy) {
        ESP_LOGE(TAG, "Failed to duplicate screen config JSON");
        return ESP_ERR_NO_MEM;
    }

    if (!little_file_system_write_json_to_file(config_copy, SCREEN_CONFIG_FILE_NAME)) {
        ESP_LOGE(TAG, "Failed to save screen config");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Screen config saved");
    return ESP_OK;
}

static void screen_post_config_changed_event(void)
{
    esp_err_t event_ret = esp_event_post(SCREEN_EVENT, SCREEN_EVENT_CONFIG_CHANGED, NULL, 0, portMAX_DELAY);
    if (event_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post screen config changed event: %s", esp_err_to_name(event_ret));
    }
}

esp_err_t screen_save_config(const cJSON *config)
{
    SemaphoreHandle_t lock = screen_get_config_lock();
    if (!lock) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTakeRecursive(lock, portMAX_DELAY);
    esp_err_t err = screen_save_config_locked(config);
    xSemaphoreGiveRecursive(lock);
    if (err == ESP_OK) {
        screen_post_config_changed_event();
    }
    return err;
}

static cJSON *screen_load_config_locked(void)
{
    cJSON *config = little_file_system_read_file_to_json(SCREEN_CONFIG_FILE_NAME);
    if (config) {
        return config;
    }

    config = cJSON_CreateObject();
    if (!config) {
        ESP_LOGE(TAG, "Failed to create default screen config");
        return NULL;
    }

    cJSON *screens = cJSON_CreateArray();
    if (!screens) {
        ESP_LOGE(TAG, "Failed to create default screens array");
        cJSON_Delete(config);
        return NULL;
    }

    cJSON_AddItemToObject(config, "screens", screens);
    return config;
}

cJSON *screen_load_config(void)
{
    SemaphoreHandle_t lock = screen_get_config_lock();
    if (!lock) {
        return NULL;
    }
    xSemaphoreTakeRecursive(lock, portMAX_DELAY);
    cJSON *config = screen_load_config_locked();
    xSemaphoreGiveRecursive(lock);
    return config;
}

void screen_free_entity_ids(char **entity_ids, size_t entity_id_count)
{
    for (size_t i = 0; i < entity_id_count; i++) {
        free(entity_ids[i]);
    }
    free(entity_ids);
}

static bool screen_entity_id_exists(char *const *entity_ids,
                                    size_t entity_id_count,
                                    const char *entity_id)
{
    for (size_t i = 0; i < entity_id_count; i++) {
        if (strcmp(entity_ids[i], entity_id) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t screen_add_unique_entity_id(char ***entity_ids,
                                             size_t *entity_id_count,
                                             const char *entity_id)
{
    if (!entity_id || !entity_id[0] ||
        screen_entity_id_exists(*entity_ids, *entity_id_count, entity_id)) {
        return ESP_OK;
    }

    char **resized = realloc(*entity_ids,
                             (*entity_id_count + 1) * sizeof(**entity_ids));
    if (!resized) {
        return ESP_ERR_NO_MEM;
    }
    *entity_ids = resized;
    resized[*entity_id_count] = strdup(entity_id);
    if (!resized[*entity_id_count]) {
        return ESP_ERR_NO_MEM;
    }
    (*entity_id_count)++;
    return ESP_OK;
}

static esp_err_t screen_collect_entities_from_array(const cJSON *items,
                                                    char ***entity_ids,
                                                    size_t *entity_id_count)
{
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, items) {
        const cJSON *data = cJSON_GetObjectItemCaseSensitive(item, "data");
        const cJSON *entity = cJSON_GetObjectItemCaseSensitive(data, "entity");
        if (cJSON_IsString(entity) && entity->valuestring) {
            esp_err_t err = screen_add_unique_entity_id(entity_ids,
                                                        entity_id_count,
                                                        entity->valuestring);
            if (err != ESP_OK) {
                return err;
            }
        }
    }
    return ESP_OK;
}

esp_err_t screen_get_configured_entity_ids(char ***entity_ids_out,
                                           size_t *entity_id_count_out)
{
    if (!entity_ids_out || !entity_id_count_out) {
        return ESP_ERR_INVALID_ARG;
    }
    *entity_ids_out = NULL;
    *entity_id_count_out = 0;

    SemaphoreHandle_t lock = screen_get_config_lock();
    if (!lock) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTakeRecursive(lock, portMAX_DELAY);
    cJSON *config = screen_load_config_locked();
    if (!config) {
        xSemaphoreGiveRecursive(lock);
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    const cJSON *screens = cJSON_GetObjectItemCaseSensitive(config, "screens");
    const cJSON *screen = NULL;
    cJSON_ArrayForEach(screen, screens) {
        err = screen_collect_entities_from_array(
            cJSON_GetObjectItemCaseSensitive(screen, "badges"),
            entity_ids_out,
            entity_id_count_out);
        if (err == ESP_OK) {
            err = screen_collect_entities_from_array(
                cJSON_GetObjectItemCaseSensitive(screen, "cards"),
                entity_ids_out,
                entity_id_count_out);
        }
        if (err != ESP_OK) {
            break;
        }
    }
    cJSON_Delete(config);
    xSemaphoreGiveRecursive(lock);

    if (err != ESP_OK) {
        screen_free_entity_ids(*entity_ids_out, *entity_id_count_out);
        *entity_ids_out = NULL;
        *entity_id_count_out = 0;
    }
    return err;
}

static bool screen_entity_id_matches(const char *entity_id,
                                     const char *const *entity_ids,
                                     size_t entity_id_count)
{
    if (!entity_id) {
        return false;
    }
    for (size_t i = 0; i < entity_id_count; i++) {
        if (entity_ids[i] && strcmp(entity_id, entity_ids[i]) == 0) {
            return true;
        }
    }
    return false;
}

static size_t screen_remove_entities_from_array(cJSON *items,
                                                const char *const *entity_ids,
                                                size_t entity_id_count)
{
    size_t removed = 0;
    for (int i = cJSON_GetArraySize(items) - 1; i >= 0; i--) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        cJSON *data = cJSON_GetObjectItemCaseSensitive(item, "data");
        cJSON *entity = cJSON_GetObjectItemCaseSensitive(data, "entity");
        if (cJSON_IsString(entity) && entity->valuestring &&
            screen_entity_id_matches(entity->valuestring,
                                     entity_ids,
                                     entity_id_count)) {
            cJSON_DeleteItemFromArray(items, i);
            removed++;
        }
    }
    return removed;
}

esp_err_t screen_remove_entity_references(const char *const *entity_ids,
                                          size_t entity_id_count,
                                          size_t *removed_reference_count_out)
{
    if (!entity_ids || entity_id_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (removed_reference_count_out) {
        *removed_reference_count_out = 0;
    }

    SemaphoreHandle_t lock = screen_get_config_lock();
    if (!lock) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTakeRecursive(lock, portMAX_DELAY);
    cJSON *config = screen_load_config_locked();
    if (!config) {
        xSemaphoreGiveRecursive(lock);
        return ESP_FAIL;
    }

    size_t removed = 0;
    cJSON *screens = cJSON_GetObjectItemCaseSensitive(config, "screens");
    cJSON *screen = NULL;
    cJSON_ArrayForEach(screen, screens) {
        removed += screen_remove_entities_from_array(
            cJSON_GetObjectItemCaseSensitive(screen, "badges"),
            entity_ids,
            entity_id_count);
        removed += screen_remove_entities_from_array(
            cJSON_GetObjectItemCaseSensitive(screen, "cards"),
            entity_ids,
            entity_id_count);
    }

    esp_err_t err = removed > 0 ? screen_save_config_locked(config) : ESP_OK;
    cJSON_Delete(config);
    xSemaphoreGiveRecursive(lock);
    if (err == ESP_OK && removed > 0) {
        screen_post_config_changed_event();
    }
    if (removed_reference_count_out) {
        *removed_reference_count_out = err == ESP_OK ? removed : 0;
    }
    return err;
}

esp_err_t screen_reset_config(void)
{
    cJSON *config = cJSON_CreateObject();
    cJSON *screens = cJSON_CreateArray();
    if (!config || !screens) {
        cJSON_Delete(config);
        cJSON_Delete(screens);
        ESP_LOGE(TAG, "Failed to create empty screen config");
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddItemToObject(config, "screens", screens)) {
        cJSON_Delete(config);
        cJSON_Delete(screens);
        ESP_LOGE(TAG, "Failed to assemble empty screen config");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = screen_save_config(config);
    cJSON_Delete(config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Screen config reset after Home Assistant authorization");
    }
    return err;
}
