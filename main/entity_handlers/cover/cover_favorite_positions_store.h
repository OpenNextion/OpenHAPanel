#ifndef OPEN_NEXTION_COVER_FAVORITE_POSITIONS_STORE_H
#define OPEN_NEXTION_COVER_FAVORITE_POSITIONS_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "esp_err.h"

/* The tile card can render up to eight favorite positions in a full-width
 * landscape row; store enough entries for that layout. */
#define COVER_FAVORITE_POSITION_MAX 8

esp_err_t cover_favorite_positions_store_init(void);
void cover_favorite_positions_store_clear(void);
void cover_favorite_positions_store_remove(const char *entity_id);
void cover_favorite_positions_store_retain(const char *const *entity_ids,
                                           size_t entity_id_count);

/**
 * Update one entity from an extended Entity Registry entry.
 * Missing favorite_positions removes the custom override; an empty array is a
 * valid override that hides the favorite-position UI.
 */
esp_err_t cover_favorite_positions_store_update(const char *entity_id,
                                                const cJSON *entry,
                                                bool *changed);

/** Returns true only when HA has an explicit favorite_positions option. */
bool cover_favorite_positions_store_get(const char *entity_id,
                                        uint8_t *positions,
                                        size_t capacity,
                                        size_t *count);

/**
 * Resolve the effective favorite positions for an entity. Missing
 * favorite_positions falls back to the built-in defaults (matching the web
 * UI); an explicit empty array yields count 0 to hide the control.
 */
size_t cover_favorite_positions_store_resolve(const char *entity_id,
                                              uint8_t *positions,
                                              size_t capacity);

#endif
