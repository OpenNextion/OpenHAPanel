#ifndef OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_HANDLER_REGISTRY_H
#define OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_HANDLER_REGISTRY_H

#include "entity_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Find the handler registered for the domain portion of an entity id. */
const entity_handler_t *entity_handler_registry_find(const char *entity_id);
/** Get default action configuration for an entity based on its domain. */
void entity_handler_registry_get_default_action_config(
    const char *entity_id,
    screen_action_config_t *out_config);
/** Dispatch a configured tile action to its domain strategy and fallback. */
bool entity_handler_registry_dispatch_action(
    const entity_action_context_t *context);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_HANDLER_REGISTRY_H
