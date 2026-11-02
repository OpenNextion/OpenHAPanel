#ifndef OPEN_NEXTION_MAIN_ENTITY_HANDLERS_LIGHT_ENTITY_HANDLER_H
#define OPEN_NEXTION_MAIN_ENTITY_HANDLERS_LIGHT_ENTITY_HANDLER_H

#include "../entity_handler.h"

extern const entity_handler_t light_entity_handler;
/** Shared tile/feature color derived from the current light color mode. */
lv_color_t light_entity_active_color(ha_entity_store_t *store,
                                     const char *entity_id);

#endif // OPEN_NEXTION_MAIN_ENTITY_HANDLERS_LIGHT_ENTITY_HANDLER_H
