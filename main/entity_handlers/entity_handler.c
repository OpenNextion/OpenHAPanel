#include "entity_handler.h"
#include "entity_handler_registry.h"

#include <stdio.h>
#include <string.h>

bool entity_handler_snapshot_is_unavailable(const ha_entity_snapshot_t *snapshot)
{
    return !snapshot ||
           snapshot->sync_status == HA_ENTITY_SYNC_REMOVED ||
           strcmp(snapshot->state, "unknown") == 0 ||
           strcmp(snapshot->state, "unavailable") == 0;
}

bool entity_handler_snapshot_is_service_unavailable(
    const ha_entity_snapshot_t *snapshot)
{
    return !snapshot ||
           snapshot->sync_status == HA_ENTITY_SYNC_REMOVED ||
           strcmp(snapshot->state, "unavailable") == 0;
}

void entity_handler_apply_friendly_name(const ha_entity_snapshot_t *snapshot,
                                        entity_display_model_t *model)
{
    if (!snapshot || !model || !snapshot->friendly_name[0]) {
        return;
    }

    snprintf(model->title, sizeof(model->title), "%s", snapshot->friendly_name);
}

void entity_handler_copy_snapshot_state(char *buffer,
                                        size_t buffer_size,
                                        const ha_entity_snapshot_t *snapshot)
{
    if (!buffer || buffer_size == 0) {
        return;
    }

    if (!snapshot || snapshot->sync_status == HA_ENTITY_SYNC_REMOVED) {
        snprintf(buffer, buffer_size, "Unavailable");
        return;
    }

    entity_handler_copy_state_text(buffer, buffer_size, snapshot->state);
}

void entity_handler_copy_state_text(char *buffer,
                                    size_t buffer_size,
                                    const char *state)
{
    if (!buffer || buffer_size == 0) {
        return;
    }

    snprintf(buffer, buffer_size, "%s", state ? state : "");
    if (buffer[0] >= 'a' && buffer[0] <= 'z') {
        buffer[0] = (char)(buffer[0] - 'a' + 'A');
    }
}

const screen_action_t *entity_handler_get_action(
    const entity_action_context_t *context)
{
    if (!context) {
        return NULL;
    }

    const screen_action_config_t *config = context->action_config;
    if (config) {
        switch (context->trigger) {
        case ENTITY_ACTION_TRIGGER_TAP:
            if (config->has_tap_action) {
                return &config->tap_action;
            }
            break;
        case ENTITY_ACTION_TRIGGER_HOLD:
            if (config->has_hold_action) {
                return &config->hold_action;
            }
            break;
        case ENTITY_ACTION_TRIGGER_ICON_TAP:
            if (config->has_icon_tap_action) {
                return &config->icon_tap_action;
            }
            break;
        default:
            return NULL;
        }
    }

    /* Fallback to handler default action if not configured in action_config */
    if (context->entity_id) {
        const entity_handler_t *handler = entity_handler_registry_find(context->entity_id);
        if (handler) {
            switch (context->trigger) {
            case ENTITY_ACTION_TRIGGER_TAP:
                if (handler->default_action_config.has_tap_action) {
                    return &handler->default_action_config.tap_action;
                }
                break;
            case ENTITY_ACTION_TRIGGER_HOLD:
                if (handler->default_action_config.has_hold_action) {
                    return &handler->default_action_config.hold_action;
                }
                break;
            case ENTITY_ACTION_TRIGGER_ICON_TAP:
                if (handler->default_action_config.has_icon_tap_action) {
                    return &handler->default_action_config.icon_tap_action;
                }
                break;
            default:
                break;
            }
        }
    }

    return NULL;
}

bool entity_handler_handle_more_info_action(
    const entity_action_context_t *context)
{
    const screen_action_t *action = entity_handler_get_action(context);
    if (!action || action->action != SCREEN_ACTION_MORE_INFO ||
        !context->open_more_info || !context->entity_id) {
        return false;
    }

    context->open_more_info(context->entity_id, context->user_data);
    return true;
}
