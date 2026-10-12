#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_H
#define OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_H

#include <stddef.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_event.h"

#define SCREEN_CONFIG_FILE_NAME "screen.json"

ESP_EVENT_DECLARE_BASE(SCREEN_EVENT);

typedef enum {
    SCREEN_EVENT_CONFIG_CHANGED,
} screen_event_t;

esp_err_t screen_save_config(const cJSON *config);
cJSON *screen_load_config(void);
/** Reset screen.json to a valid configuration containing no screens. */
esp_err_t screen_reset_config(void);

/**
 * Return the unique entity IDs referenced by all screen badges and tile cards.
 * The caller owns the returned array and must release it with
 * screen_free_entity_ids().
 */
esp_err_t screen_get_configured_entity_ids(char ***entity_ids_out,
                                           size_t *entity_id_count_out);
void screen_free_entity_ids(char **entity_ids, size_t entity_id_count);

/**
 * Remove every badge and tile-card reference to any supplied entity ID and
 * persist the updated configuration. A single entity may be removed from
 * multiple split screens. No file write or config-changed event occurs when
 * no matching references exist.
 */
esp_err_t screen_remove_entity_references(const char *const *entity_ids,
                                          size_t entity_id_count,
                                          size_t *removed_reference_count_out);

#endif // OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_H

#ifdef __cplusplus
}
#endif
