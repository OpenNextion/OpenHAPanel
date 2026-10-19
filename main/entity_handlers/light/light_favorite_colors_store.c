#include "light_favorite_colors_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ha_entity_store_type.h"

typedef struct light_favorite_colors_entry {
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    light_favorite_color_t colors[LIGHT_FAVORITE_COLOR_MAX];
    size_t count;
    struct light_favorite_colors_entry *next;
} light_favorite_colors_entry_t;

static const char *TAG = "light_favorite_colors";
static SemaphoreHandle_t s_lock;
static light_favorite_colors_entry_t *s_entries;

static const cJSON *light_favorite_colors_array(const cJSON *entry)
{
    const cJSON *options = cJSON_GetObjectItemCaseSensitive(entry, "options");
    const cJSON *light = cJSON_GetObjectItemCaseSensitive(options, "light");
    const cJSON *favorites =
        cJSON_GetObjectItemCaseSensitive(light, "favorite_colors");
    return cJSON_IsArray(favorites) ? favorites : NULL;
}

static bool light_parse_byte_array(const cJSON *array,
                                   uint8_t *values,
                                   size_t count)
{
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != (int)count) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(array, (int)i);
        if (!cJSON_IsNumber(item) || item->valuedouble < 0.0 ||
            item->valuedouble > 255.0) {
            return false;
        }
        values[i] = (uint8_t)(item->valuedouble + 0.5);
    }
    return true;
}

static bool light_parse_favorite(const cJSON *json,
                                 light_favorite_color_t *color)
{
    if (!cJSON_IsObject(json) || !color) {
        return false;
    }
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(
        json,
        "color_temp_kelvin");
    if (cJSON_IsNumber(value) && value->valuedouble >= 1.0 &&
        value->valuedouble <= 65535.0) {
        color->type = LIGHT_FAVORITE_COLOR_TEMP;
        color->value.color_temp_kelvin =
            (uint16_t)(value->valuedouble + 0.5);
        return true;
    }

    value = cJSON_GetObjectItemCaseSensitive(json, "hs_color");
    if (cJSON_IsArray(value) && cJSON_GetArraySize(value) == 2) {
        const cJSON *hue = cJSON_GetArrayItem(value, 0);
        const cJSON *saturation = cJSON_GetArrayItem(value, 1);
        if (cJSON_IsNumber(hue) && cJSON_IsNumber(saturation) &&
            hue->valuedouble >= 0.0 && hue->valuedouble <= 360.0 &&
            saturation->valuedouble >= 0.0 &&
            saturation->valuedouble <= 100.0) {
            color->type = LIGHT_FAVORITE_HS;
            color->value.hs.hue = hue->valuedouble;
            color->value.hs.saturation = saturation->valuedouble;
            return true;
        }
    }

    uint8_t values[5] = {0};
    value = cJSON_GetObjectItemCaseSensitive(json, "rgb_color");
    if (light_parse_byte_array(value, values, 3)) {
        color->type = LIGHT_FAVORITE_RGB;
        color->value.rgb = (light_rgb_t) {values[0], values[1], values[2]};
        return true;
    }
    value = cJSON_GetObjectItemCaseSensitive(json, "rgbw_color");
    if (light_parse_byte_array(value, values, 4)) {
        color->type = LIGHT_FAVORITE_RGBW;
        color->value.rgbw = (light_rgbw_t) {
            values[0], values[1], values[2], values[3]
        };
        return true;
    }
    value = cJSON_GetObjectItemCaseSensitive(json, "rgbww_color");
    if (light_parse_byte_array(value, values, 5)) {
        color->type = LIGHT_FAVORITE_RGBWW;
        color->value.rgbww = (light_rgbww_t) {
            values[0], values[1], values[2], values[3], values[4]
        };
        return true;
    }
    return false;
}

static light_favorite_colors_entry_t **light_find_entry(const char *entity_id)
{
    light_favorite_colors_entry_t **cursor = &s_entries;
    while (*cursor && strcmp((*cursor)->entity_id, entity_id) != 0) {
        cursor = &(*cursor)->next;
    }
    return cursor;
}

static bool light_favorite_equal(const light_favorite_color_t *left,
                                 const light_favorite_color_t *right)
{
    if (!left || !right || left->type != right->type) {
        return false;
    }
    switch (left->type) {
    case LIGHT_FAVORITE_COLOR_TEMP:
        return left->value.color_temp_kelvin ==
               right->value.color_temp_kelvin;
    case LIGHT_FAVORITE_RGB:
        return memcmp(&left->value.rgb,
                      &right->value.rgb,
                      sizeof(left->value.rgb)) == 0;
    case LIGHT_FAVORITE_HS:
        return left->value.hs.hue == right->value.hs.hue &&
               left->value.hs.saturation == right->value.hs.saturation;
    case LIGHT_FAVORITE_RGBW:
        return memcmp(&left->value.rgbw,
                      &right->value.rgbw,
                      sizeof(left->value.rgbw)) == 0;
    case LIGHT_FAVORITE_RGBWW:
        return memcmp(&left->value.rgbww,
                      &right->value.rgbww,
                      sizeof(left->value.rgbww)) == 0;
    }
    return false;
}

static bool light_favorite_list_equal(
    const light_favorite_colors_entry_t *left,
    const light_favorite_colors_entry_t *right)
{
    if (!left || !right || left->count != right->count) {
        return false;
    }
    for (size_t i = 0; i < left->count; i++) {
        if (!light_favorite_equal(&left->colors[i], &right->colors[i])) {
            return false;
        }
    }
    return true;
}

esp_err_t light_favorite_colors_store_init(void)
{
    if (s_lock) {
        return ESP_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

void light_favorite_colors_store_clear(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    light_favorite_colors_entry_t *entry = s_entries;
    s_entries = NULL;
    xSemaphoreGive(s_lock);
    while (entry) {
        light_favorite_colors_entry_t *next = entry->next;
        heap_caps_free(entry);
        entry = next;
    }
}

void light_favorite_colors_store_remove(const char *entity_id)
{
    if (!s_lock || !entity_id) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    light_favorite_colors_entry_t **slot = light_find_entry(entity_id);
    light_favorite_colors_entry_t *removed = *slot;
    if (removed) {
        *slot = removed->next;
    }
    xSemaphoreGive(s_lock);
    heap_caps_free(removed);
}

void light_favorite_colors_store_retain(const char *const *entity_ids,
                                        size_t entity_id_count)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    light_favorite_colors_entry_t **slot = &s_entries;
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
        light_favorite_colors_entry_t *removed = *slot;
        *slot = removed->next;
        heap_caps_free(removed);
    }
    xSemaphoreGive(s_lock);
}

esp_err_t light_favorite_colors_store_update(const char *entity_id,
                                             const cJSON *entry,
                                             bool *changed)
{
    if (!s_lock || !entity_id || !entry) {
        return ESP_ERR_INVALID_ARG;
    }
    if (changed) {
        *changed = false;
    }

    const cJSON *favorites = light_favorite_colors_array(entry);
    light_favorite_colors_entry_t *replacement = NULL;
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
            if (replacement->count >= LIGHT_FAVORITE_COLOR_MAX) {
                continue;
            }
            light_favorite_color_t color = {0};
            if (light_parse_favorite(item, &color)) {
                replacement->colors[replacement->count++] = color;
            }
            else {
                ESP_LOGW(TAG,
                         "Ignoring invalid favorite color for %s at index %u",
                         entity_id,
                         (unsigned)(source_count - 1));
            }
        }
        if (source_count > LIGHT_FAVORITE_COLOR_MAX) {
            ESP_LOGW(TAG,
                     "Favorite colors for %s truncated from %u to %u",
                     entity_id,
                     (unsigned)source_count,
                     (unsigned)LIGHT_FAVORITE_COLOR_MAX);
        }
    }

    const bool has_override = replacement != NULL;
    const size_t replacement_count = replacement ? replacement->count : 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    light_favorite_colors_entry_t **slot = light_find_entry(entity_id);
    light_favorite_colors_entry_t *previous = *slot;
    bool is_changed = false;
    if ((previous == NULL) != (replacement == NULL)) {
        is_changed = true;
    }
    else if (previous && replacement &&
             !light_favorite_list_equal(previous, replacement)) {
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
                     "Cached %u custom favorite color(s) for %s",
                     (unsigned)replacement_count,
                     entity_id);
        }
        else {
            ESP_LOGI(TAG,
                     "Using default favorite colors for %s",
                     entity_id);
        }
    }
    return ESP_OK;
}

bool light_favorite_colors_store_get(const char *entity_id,
                                     light_favorite_color_t *colors,
                                     size_t capacity,
                                     size_t *count)
{
    if (!s_lock || !entity_id || !count || (!colors && capacity > 0)) {
        return false;
    }
    *count = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    light_favorite_colors_entry_t **slot = light_find_entry(entity_id);
    const light_favorite_colors_entry_t *entry = *slot;
    if (!entry) {
        xSemaphoreGive(s_lock);
        return false;
    }
    *count = entry->count < capacity ? entry->count : capacity;
    if (*count > 0) {
        memcpy(colors, entry->colors, *count * sizeof(colors[0]));
    }
    xSemaphoreGive(s_lock);
    return true;
}
