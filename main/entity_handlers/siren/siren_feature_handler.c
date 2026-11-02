#include "siren_feature_handler.h"

#include <string.h>

#include "app.h"
#include "assets/mdi_symbol_def.h"
#include "common/ha_control_toggle/ha_control_toggle.h"
#include "esp_log.h"

static const char *TAG = "siren_feature_handler";

static bool siren_feature_supports(ha_entity_store_t *store,
                                   const char *entity_id,
                                   screen_feature_type_t type)
{
    (void)store;
    (void)entity_id;
    return type == SCREEN_FEATURE_SIREN_TOGGLE;
}

static void toggle_changed(bool on, void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store ||
        ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) {
        return;
    }
    const char *service = on ? "turn_on" : "turn_off";
    esp_err_t err = app_ha_call_entity_service("siren", service,
                                                instance->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "siren.%s for %s failed: %s",
                 service, instance->entity_id, esp_err_to_name(err));
    }
}

static esp_err_t siren_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config ||
        instance->config->type != SCREEN_FEATURE_SIREN_TOGGLE) {
        return ESP_ERR_INVALID_ARG;
    }
    instance->control = ha_control_toggle_create(instance->host);
    if (!instance->control) {
        return ESP_ERR_NO_MEM;
    }
    ha_control_toggle_set_disabled(instance->control, true);
    ha_control_toggle_set_change_cb(instance->control, toggle_changed,
                                    instance);
    ha_control_toggle_set_icon_text(instance->control, MDI_SYMBOL_VOLUME_OFF);
    return ESP_OK;
}

static void siren_feature_update(entity_feature_instance_t *instance,
                                 ha_entity_store_t *store,
                                 const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) {
        return;
    }
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool on = available && strcmp(snapshot->state, "on") == 0;
    ha_control_toggle_set_active_color(instance->control,
                                       lv_color_hex(0xF44336));
    ha_control_toggle_set_value(instance->control, on, LV_ANIM_ON);
    ha_control_toggle_set_disabled(instance->control, !available);
    ha_control_toggle_set_icon_text(instance->control,
                                    on ? MDI_SYMBOL_VOLUME_HIGH
                                       : MDI_SYMBOL_VOLUME_OFF);
}

const entity_feature_handler_t siren_feature_handler = {
    .supports = siren_feature_supports,
    .create = siren_feature_create,
    .update = siren_feature_update,
};
