#include "entity_handler_registry.h"

#include <string.h>

#include "binary_sensor/binary_sensor_entity_handler.h"
#include "button/button_entity_handler.h"
#include "climate/climate_entity_handler.h"
#include "cover/cover_entity_handler.h"
#include "light/light_entity_handler.h"
#include "scene/scene_entity_handler.h"
#include "switch/switch_entity_handler.h"
#include "sensor/sensor_entity_handler.h"
#include "event/event_entity_handler.h"
#include "siren/siren_entity_handler.h"

static const entity_handler_t *const s_entity_handlers[] = {
    &binary_sensor_entity_handler,
    &button_entity_handler,
    &climate_entity_handler,
    &light_entity_handler,
    &switch_entity_handler,
    &cover_entity_handler,
    &scene_entity_handler,
    &sensor_entity_handler,
    &event_entity_handler,
    &siren_entity_handler,
};

const entity_handler_t *entity_handler_registry_find(const char *entity_id)
{
    if (!entity_id) {
        return NULL;
    }

    const char *separator = strchr(entity_id, '.');
    if (!separator || separator == entity_id) {
        return NULL;
    }

    const size_t domain_length = (size_t)(separator - entity_id);
    for (size_t i = 0; i < sizeof(s_entity_handlers) / sizeof(s_entity_handlers[0]); i++) {
        const entity_handler_t *handler = s_entity_handlers[i];
        if (strlen(handler->domain) == domain_length &&
            strncmp(entity_id, handler->domain, domain_length) == 0) {
            return handler;
        }
    }

    return NULL;
}

void entity_handler_registry_get_default_action_config(
    const char *entity_id,
    screen_action_config_t *out_config)
{
    if (!out_config) {
        return;
    }

    memset(out_config, 0, sizeof(*out_config));

    const entity_handler_t *handler = entity_handler_registry_find(entity_id);
    if (handler) {
        *out_config = handler->default_action_config;
        return;
    }

    /* Fallback default for unknown domain */
    out_config->has_tap_action = true;
    out_config->tap_action.action = SCREEN_ACTION_MORE_INFO;
    out_config->has_hold_action = true;
    out_config->hold_action.action = SCREEN_ACTION_NOTHING;
    out_config->has_icon_tap_action = true;
    out_config->icon_tap_action.action = SCREEN_ACTION_NOTHING;
}

bool entity_handler_registry_dispatch_action(
    const entity_action_context_t *context)
{
    if (!context || !context->entity_id) {
        return false;
    }

    const entity_handler_t *handler = entity_handler_registry_find(
        context->entity_id);
    if (!handler || !handler->handle_action) {
        return false;
    }

    return handler->handle_action(context);
}
