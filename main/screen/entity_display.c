#include "entity_display.h"

#include <stdio.h>
#include <string.h>

#include "assets/mdi_symbol_def.h"
#include "entity_handlers/entity_handler_registry.h"
#include "ha_entity_store.h"
#include "lvgl.h"

static esp_err_t entity_display_build_default(const ha_entity_snapshot_t *snapshot,
                                              entity_display_model_t *model);

uint32_t entity_display_required_fields(const char *entity_id)
{
    const entity_handler_t *handler = entity_handler_registry_find(entity_id);
    return handler ? handler->required_fields : HA_ENTITY_FIELD_ALL;
}

bool entity_display_should_refresh(const char *entity_id, uint32_t changes)
{
    const entity_handler_t *handler = entity_handler_registry_find(entity_id);
    const uint32_t refresh_changes = handler
                                         ? handler->refresh_changes
                                         : ENTITY_HANDLER_REFRESH_CHANGES;
    return (refresh_changes & changes) != 0;
}

void entity_display_model_init(const char *entity_id,
                               entity_display_model_t *out_model)
{
    if (!out_model) {
        return;
    }

    const entity_handler_t *handler = entity_handler_registry_find(entity_id);
    memset(out_model, 0, sizeof(*out_model));
    snprintf(out_model->title,
             sizeof(out_model->title),
             "%s",
             entity_id ? entity_id : "");
    out_model->icon = handler && handler->initial_icon
                          ? handler->initial_icon
                          : MDI_SYMBOL_HOME;
    out_model->background_color = lv_color_hex(0x262626);
    out_model->icon_color = lv_color_hex(0x4B5058);
    out_model->icon_circle_background_color = lv_color_hex(0xECEFF1);
    out_model->title_color = lv_color_hex(0xFFFFFF);
    out_model->state_color = lv_color_hex(0xA8A8A8);
}

esp_err_t entity_display_build(ha_entity_store_t *store,
                               const char *entity_id,
                               entity_display_model_t *out_model)
{
    if (!store || !entity_id || !out_model) {
        return ESP_ERR_INVALID_ARG;
    }

    entity_display_model_init(entity_id, out_model);

    ha_entity_snapshot_t snapshot = {0};
    esp_err_t err = ha_entity_store_get(store, entity_id, &snapshot);
    if (err != ESP_OK) {
        return err;
    }

    /* The warning icon represents an explicitly unavailable/removed entity.
     * "unknown" is still a valid initial state for stateless entities such as
     * button and scene, so it must not enable the card-wide warning badge. */
    out_model->unavailable =
        entity_handler_snapshot_is_service_unavailable(&snapshot);

    const entity_handler_t *handler = entity_handler_registry_find(entity_id);
    if (!handler || !handler->render_tile) {
        return entity_display_build_default(&snapshot, out_model);
    }

    return handler->render_tile(store, &snapshot, out_model);
}

static esp_err_t entity_display_build_default(const ha_entity_snapshot_t *snapshot,
                                              entity_display_model_t *model)
{
    entity_handler_apply_friendly_name(snapshot, model);
    entity_handler_copy_snapshot_state(model->state_text,
                                       sizeof(model->state_text),
                                       snapshot);

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        model->icon = MDI_SYMBOL_HOME;
        model->background_color = lv_color_hex(0x242424);
        model->icon_color = lv_color_hex(0xA0A0A0);
        model->icon_circle_background_color = lv_color_hex(0x505050);
        model->title_color = lv_color_hex(0x909090);
        model->state_color = lv_color_hex(0x808080);
    }

    return ESP_OK;
}
