// main/entity_handlers/button/button_feature_handler.c
#include "button_feature_handler.h"

#include <string.h>

#include "app.h"
#include "common/ha_control_button/ha_control_button.h"
#include "esp_log.h"

static const char *TAG = "button_feature_handler";

static bool button_feature_supports(ha_entity_store_t *store,
                                    const char *entity_id,
                                    screen_feature_type_t type)
{
    (void)store;
    (void)entity_id;
    return type == SCREEN_FEATURE_BUTTON;
}

static void button_pressed(void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id) {
        return;
    }
    esp_err_t err = app_ha_call_entity_service("button", "press",
                                                instance->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "button.press for %s failed: %s",
                 instance->entity_id, esp_err_to_name(err));
    }
}

static esp_err_t button_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config ||
        instance->config->type != SCREEN_FEATURE_BUTTON) {
        return ESP_ERR_INVALID_ARG;
    }
    instance->control = ha_control_button_create(instance->host);
    if (!instance->control) {
        return ESP_ERR_NO_MEM;
    }
    ha_control_button_set_disabled(instance->control, true);
    ha_control_button_set_press_cb(instance->control, button_pressed,
                                   instance);
    return ESP_OK;
}

static void button_feature_update(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) {
        return;
    }
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_service_unavailable(snapshot);
    ha_control_button_set_disabled(instance->control, !available);
}

static void button_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (instance && instance->control) {
        ha_control_button_refresh_theme(instance->control);
    }
}

const entity_feature_handler_t button_feature_handler = {
    .supports = button_feature_supports,
    .create = button_feature_create,
    .update = button_feature_update,
    .apply_theme = button_feature_apply_theme,
};
