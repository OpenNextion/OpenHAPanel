#ifndef OPEN_NEXTION_MAIN_SCREEN_BADGE_DISPLAY_H
#define OPEN_NEXTION_MAIN_SCREEN_BADGE_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>

#include "entity_display.h"

bool badge_display_supported(const char *entity_id);

/** Format the compact badge text and apply badge-only icon rules to model. */
void badge_display_format(ha_entity_store_t *store,
                          const char *entity_id,
                          const char *configured_name,
                          entity_display_model_t *model,
                          char *text,
                          size_t text_size);

#endif // OPEN_NEXTION_MAIN_SCREEN_BADGE_DISPLAY_H
