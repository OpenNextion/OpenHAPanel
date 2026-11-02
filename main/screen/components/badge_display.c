#include "entity_handlers/entity_state_translation.h"
#include "i18n.h"
#include "badge_display.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets/mdi_symbol_def.h"
#include "entity_handlers/entity_handler.h"
#include "entity_handlers/entity_handler_registry.h"
#include "ha_entity_store.h"

static bool badge_domain_is(const char *entity_id, const char *domain)
{
    const size_t len = strlen(domain);
    return entity_id && strncmp(entity_id, domain, len) == 0 &&
           entity_id[len] == '.';
}

bool badge_display_supported(const char *entity_id)
{
    return entity_handler_registry_find(entity_id) != NULL ||
           badge_domain_is(entity_id, "fan");
}

static void badge_copy_name(const char *name, char *text, size_t text_size)
{
    char display_name[128];
    i18n_display_copy(display_name, sizeof(display_name), name);
    name = display_name;
    /* Limit by UTF-8 characters so a Chinese name is never cut mid-character. */
    size_t bytes = 0;
    size_t characters = 0;
    while (name[bytes] && characters < 10) {
        const unsigned char lead = (unsigned char)name[bytes];
        size_t length = lead < 0x80 ? 1 :
                        (lead & 0xE0) == 0xC0 ? 2 :
                        (lead & 0xF0) == 0xE0 ? 3 :
                        (lead & 0xF8) == 0xF0 ? 4 : 1;
        for (size_t i = 1; i < length; i++) {
            if (!name[bytes + i] ||
                ((unsigned char)name[bytes + i] & 0xC0) != 0x80) {
                length = 1;
                break;
            }
        }
        bytes += length;
        characters++;
    }
    const bool truncated = name[bytes] != '\0';
    if (text_size == 0) {
        return;
    }
    if (truncated && bytes + 4 <= text_size) {
        memcpy(text, name, bytes);
        memcpy(text + bytes, "...", 4);
    } else {
        snprintf(text, text_size, "%.*s", (int)bytes, name);
    }
}

static bool badge_get_percent(ha_entity_store_t *store,
                              const char *entity_id,
                              const char *attribute,
                              double divisor,
                              char *text,
                              size_t text_size)
{
    double value = 0;
    if (!store || ha_entity_store_get_attr_number(store, entity_id,
                                                  attribute, &value) != ESP_OK ||
        !isfinite(value)) {
        return false;
    }
    value = LV_CLAMP(0.0, value * 100.0 / divisor, 100.0);
    snprintf(text, text_size, "%.0f%%", value);
    return true;
}

static bool badge_cover_supports_position(ha_entity_store_t *store,
                                          const char *entity_id)
{
    double features = 0;
    return ha_entity_store_get_attr_number(store, entity_id,
                                           "supported_features", &features) == ESP_OK &&
           isfinite(features) && features >= 0 &&
           ((uint32_t)features & (1U << 2)) != 0;
}

void badge_display_format(ha_entity_store_t *store,
                          const char *entity_id,
                          const char *configured_name,
                          entity_display_model_t *model,
                          char *text,
                          size_t text_size)
{
    if (!entity_id || !model || !text || text_size == 0) {
        return;
    }
    snprintf(text, text_size, "%s",
             model->state_text[0] ? model->state_text : model->title);

    if (badge_domain_is(entity_id, "button") ||
        badge_domain_is(entity_id, "scene")||
        badge_domain_is(entity_id, "event")
        ) {
        if (model->unavailable && model->state_text[0]) {
            snprintf(text, text_size, "%s", model->state_text);
        } else {
            badge_copy_name(configured_name && configured_name[0]
                                ? configured_name : model->title,
                            text, text_size);
        }
        return;
    }

    ha_entity_snapshot_t snapshot = {0};
    if (!store || ha_entity_store_get(store, entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) {
        // if (badge_domain_is(entity_id, "fan")) {
            // model->icon = MDI_SYMBOL_FAN;
        // }
        return;
    }

    if (badge_domain_is(entity_id, "light")) {
        if (strcmp(snapshot.state, "on") == 0) {
            if (!badge_get_percent(store, entity_id, "brightness", 255.0,
                                   text, text_size)) {
                i18n_copy(text, text_size, i18n_get(I18N_STATE_ON));
            }
        } else if (strcmp(snapshot.state, "off") == 0) {
            i18n_copy(text, text_size, i18n_get(I18N_STATE_OFF));
        }
    } else if (badge_domain_is(entity_id, "cover")) {
        if (strcmp(snapshot.state, "open") == 0) {
            if (!badge_cover_supports_position(store, entity_id) ||
                !badge_get_percent(store, entity_id, "current_position",
                                   100.0, text, text_size)) {
                i18n_copy(text, text_size, i18n_get(I18N_STATE_OPEN));
            }
        } else if (strcmp(snapshot.state, "closed") == 0) {
            i18n_copy(text, text_size, i18n_get(I18N_STATE_CLOSED));
        } else if (strcmp(snapshot.state, "opening") == 0) {
            i18n_copy(text, text_size, i18n_get(I18N_STATE_OPENING));
        } else if (strcmp(snapshot.state, "closing") == 0) {
            i18n_copy(text, text_size, i18n_get(I18N_STATE_CLOSING));
        }
    } else if (badge_domain_is(entity_id, "binary_sensor")) {
        i18n_copy(text, text_size,
                  entity_state_translate_binary(snapshot.state, snapshot.device_class));
    } else if (badge_domain_is(entity_id, "fan")) {
        if (strcmp(snapshot.state, "on") == 0) {
            if (!badge_get_percent(store, entity_id, "percentage", 100.0,
                                   text, text_size)) {
                i18n_copy(text, text_size, i18n_get(I18N_STATE_ON));
            }
        } else if (strcmp(snapshot.state, "off") == 0) {
            i18n_copy(text, text_size, i18n_get(I18N_STATE_OFF));
        }
    }
}
