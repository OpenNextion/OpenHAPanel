#include "cover_favorite_positions_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ha_entity_store_type.h"

typedef struct cover_favorite_positions_entry {
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    uint8_t positions[COVER_FAVORITE_POSITION_MAX];
    size_t count;
    struct cover_favorite_positions_entry *next;
} cover_favorite_positions_entry_t;

static const char *TAG = "cover_favorite_positions";
static SemaphoreHandle_t s_lock;
static cover_favorite_positions_entry_t *s_entries;

static const cJSON *cover_favorite_positions_array(const cJSON *entry)
{
    const cJSON *options = cJSON_GetObjectItemCaseSensitive(entry, "options");
    const cJSON *cover = cJSON_GetObjectItemCaseSensitive(options, "cover");
    const cJSON *favorites =
        cJSON_GetObjectItemCaseSensitive(cover, "favorite_positions");
    return cJSON_IsArray(favorites) ? favorites : NULL;
}

static cover_favorite_positions_entry_t **cover_find_entry(
    const char *entity_id)
{
    cover_favorite_positions_entry_t **cursor = &s_entries;
    while (*cursor && strcmp((*cursor)->entity_id, entity_id) != 0) {
        cursor = &(*cursor)->next;
    }
    return cursor;
}

static bool cover_favorite_list_equal(
    const cover_favorite_positions_entry_t *left,
    const cover_favorite_positions_entry_t *right)
{
    if (!left || !right || left->count != right->count) {
        return false;
    }
    return left->count == 0 ||
           memcmp(left->positions,
                  right->positions,
                  left->count * sizeof(left->positions[0])) == 0;
}

esp_err_t cover_favorite_positions_store_init(void)
{
    if (s_lock) {
        return ESP_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

void cover_favorite_positions_store_clear(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cover_favorite_positions_entry_t *entry = s_entries;
    s_entries = NULL;
    xSemaphoreGive(s_lock);
    while (entry) {
        cover_favorite_positions_entry_t *next = entry->next;
        heap_caps_free(entry);
        entry = next;
    }
}

void cover_favorite_positions_store_remove(const char *entity_id)
{
    if (!s_lock || !entity_id) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cover_favorite_positions_entry_t **slot = cover_find_entry(entity_id);
    cover_favorite_positions_entry_t *removed = *slot;
    if (removed) {
        *slot = removed->next;
    }
    xSemaphoreGive(s_lock);
    heap_caps_free(removed);
}

void cover_favorite_positions_store_retain(const char *const *entity_ids,
                                           size_t entity_id_count)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cover_favorite_positions_entry_t **slot = &s_entries;
    while (*slot) {
        bool retained = false;
        for (size_t i = 0; i < entity_id_count; i++) {
            if (entity_ids && entity_ids[i] &&
                strcmp((*slot)->entity_id, entity_ids[i]) == 0) {
                retained = true;
                break;
            }
        }
        if (retained) {
            slot = &(*slot)->next;
            continue;
        }
        cover_favorite_positions_entry_t *removed = *slot;
        *slot = removed->next;
        heap_caps_free(removed);
    }
    xSemaphoreGive(s_lock);
}

esp_err_t cover_favorite_positions_store_update(const char *entity_id,
                                                const cJSON *entry,
                                                bool *changed)
{
    if (!s_lock || !entity_id || !entry) {
        return ESP_ERR_INVALID_ARG;
    }
    if (changed) {
        *changed = false;
    }

    const cJSON *favorites = cover_favorite_positions_array(entry);
    cover_favorite_positions_entry_t *replacement = NULL;
    if (favorites) {
        replacement = heap_caps_calloc(1,
                                       sizeof(*replacement),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!replacement) {
            replacement = calloc(1, sizeof(*replacement));
        }
        if (!replacement) {
            return ESP_ERR_NO_MEM;
        }
        snprintf(replacement->entity_id,
                 sizeof(replacement->entity_id),
                 "%s",
                 entity_id);
        const cJSON *item = NULL;
        size_t source_count = 0;
        cJSON_ArrayForEach(item, favorites) {
            source_count++;
            if (replacement->count >= COVER_FAVORITE_POSITION_MAX) {
                continue;
            }
            if (cJSON_IsNumber(item) && item->valuedouble >= 0.0 &&
                item->valuedouble <= 100.0) {
                replacement->positions[replacement->count++] =
                    (uint8_t)(item->valuedouble + 0.5);
            }
            else {
                ESP_LOGW(TAG,
                         "Ignoring invalid favorite position for %s at index %u",
                         entity_id,
                         (unsigned)(source_count - 1));
            }
        }
        if (source_count > COVER_FAVORITE_POSITION_MAX) {
            ESP_LOGW(TAG,
                     "Favorite positions for %s truncated from %u to %u",
                     entity_id,
                     (unsigned)source_count,
                     (unsigned)COVER_FAVORITE_POSITION_MAX);
        }
    }

    const bool has_override = replacement != NULL;
    const size_t replacement_count = replacement ? replacement->count : 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cover_favorite_positions_entry_t **slot = cover_find_entry(entity_id);
    cover_favorite_positions_entry_t *previous = *slot;
    bool is_changed = false;
    if ((previous == NULL) != (replacement == NULL)) {
        is_changed = true;
    }
    else if (previous && replacement &&
             !cover_favorite_list_equal(previous, replacement)) {
        is_changed = true;
    }
    if (replacement) {
        replacement->next = previous ? previous->next : NULL;
        *slot = replacement;
    }
    else if (previous) {
        *slot = previous->next;
    }
    xSemaphoreGive(s_lock);
    heap_caps_free(previous);
    if (changed) {
        *changed = is_changed;
    }
    if (is_changed) {
        if (has_override) {
            ESP_LOGI(TAG,
                     "Cached %u favorite position(s) for %s",
                     (unsigned)replacement_count,
                     entity_id);
        }
        else {
            ESP_LOGI(TAG,
                     "Using default favorite positions for %s",
                     entity_id);
        }
    }
    return ESP_OK;
}

bool cover_favorite_positions_store_get(const char *entity_id,
                                        uint8_t *positions,
                                        size_t capacity,
                                        size_t *count)
{
    if (!s_lock || !entity_id || !count || (!positions && capacity > 0)) {
        return false;
    }
    *count = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    cover_favorite_positions_entry_t **slot = cover_find_entry(entity_id);
    const cover_favorite_positions_entry_t *entry = *slot;
    if (!entry) {
        xSemaphoreGive(s_lock);
        return false;
    }
    *count = entry->count < capacity ? entry->count : capacity;
    if (*count > 0) {
        memcpy(positions, entry->positions, *count * sizeof(positions[0]));
    }
    xSemaphoreGive(s_lock);
    return true;
}

size_t cover_favorite_positions_store_resolve(const char *entity_id,
                                              uint8_t *positions,
                                              size_t capacity)
{
    /* Keep in sync with the web UI's DEFAULT_COVER_FAVORITE_POSITIONS. */
    static const uint8_t defaults[] = {0, 25, 75, 100};
    size_t count = 0;
    if (cover_favorite_positions_store_get(entity_id, positions, capacity,
                                           &count)) {
        return count;
    }
    if (!positions || capacity == 0) {
        return 0;
    }
    const size_t copy = sizeof(defaults) < capacity ? sizeof(defaults)
                                                    : capacity;
    memcpy(positions, defaults, copy);
    return copy;
}
