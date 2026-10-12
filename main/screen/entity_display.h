#ifndef OPEN_NEXTION_MAIN_SCREEN_ENTITY_DISPLAY_H
#define OPEN_NEXTION_MAIN_SCREEN_ENTITY_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "entity_handlers/entity_tile_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Return the Store fields required to render an entity's compact state. */
uint32_t entity_display_required_fields(const char *entity_id);

/** Return whether a Store change mask requires this entity to be rendered again. */
bool entity_display_should_refresh(const char *entity_id, uint32_t changes);

/** Initialize the fallback model used before an entity is available in Store. */
void entity_display_model_init(const char *entity_id,
                               entity_display_model_t *out_model);

/** Resolve title, state and attributes through the entity domain strategy. */
esp_err_t entity_display_build(ha_entity_store_t *store,
                               const char *entity_id,
                               entity_display_model_t *out_model);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_ENTITY_DISPLAY_H
