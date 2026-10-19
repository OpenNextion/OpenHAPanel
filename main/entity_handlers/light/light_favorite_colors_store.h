#ifndef OPEN_NEXTION_LIGHT_FAVORITE_COLORS_STORE_H
#define OPEN_NEXTION_LIGHT_FAVORITE_COLORS_STORE_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "esp_err.h"
#include "light_favorite_color.h"

esp_err_t light_favorite_colors_store_init(void);
void light_favorite_colors_store_clear(void);
void light_favorite_colors_store_remove(const char *entity_id);
void light_favorite_colors_store_retain(const char *const *entity_ids,
                                        size_t entity_id_count);

/**
 * Update one entity from an extended Entity Registry entry.
 * Missing favorite_colors removes the custom override; an empty array is a
 * valid override that hides the favorite-color UI.
 */
esp_err_t light_favorite_colors_store_update(const char *entity_id,
                                             const cJSON *entry,
                                             bool *changed);

/** Returns true only when HA has an explicit favorite_colors option. */
bool light_favorite_colors_store_get(const char *entity_id,
                                     light_favorite_color_t *colors,
                                     size_t capacity,
                                     size_t *count);

#endif
