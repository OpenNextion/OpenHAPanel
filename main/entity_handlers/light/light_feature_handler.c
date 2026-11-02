#include "light_feature_handler.h"

#include <math.h>
#include <string.h>

#include "app.h"
#include "common/ha_control_slider/ha_control_slider.h"
#include "esp_log.h"
#include "light_entity_handler.h"

static const char *TAG = "light_feature_handler";

static bool mode_supports_brightness(const char *mode)
{
    return mode && (strcmp(mode, "brightness") == 0 ||
                    strcmp(mode, "color_temp") == 0 ||
                    strcmp(mode, "white") == 0 ||
                    strcmp(mode, "hs") == 0 ||
                    strcmp(mode, "xy") == 0 ||
                    strcmp(mode, "rgb") == 0 ||
                    strcmp(mode, "rgbw") == 0 ||
                    strcmp(mode, "rgbww") == 0);
}

static bool light_feature_supports(ha_entity_store_t *store,
                                   const char *entity_id,
                                   screen_feature_type_t type)
{
    if (type != SCREEN_FEATURE_LIGHT_BRIGHTNESS) {
        return false;
    }
    if (!store || !entity_id) {
        return true; /* Show configured control until capability data arrives. */
    }
    cJSON *modes = NULL;
    if (ha_entity_store_dup_attr_json(store, entity_id,
                                      "supported_color_modes", &modes) != ESP_OK) {
        return true;
    }
    bool supported = false;
    const cJSON *mode = NULL;
    cJSON_ArrayForEach(mode, modes) {
        if (cJSON_IsString(mode) && mode_supports_brightness(mode->valuestring)) {
            supported = true;
            break;
        }
    }
    cJSON_Delete(modes);
    return supported;
}

static void brightness_changed(uint8_t percent,
                               ha_control_slider_phase_t phase,
                               void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id || phase != HA_CONTROL_SLIDER_COMMIT) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store || ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot) ||
        !light_feature_supports(store, instance->entity_id,
                                SCREEN_FEATURE_LIGHT_BRIGHTNESS)) {
        return;
    }
    esp_err_t err;
    if (percent == 0) {
        err = app_ha_call_entity_service("light", "turn_off", instance->entity_id);
    } else {
        cJSON *data = cJSON_CreateObject();
        if (!data) {
            return;
        }
        if (!cJSON_AddNumberToObject(data, "brightness_pct", percent)) {
            cJSON_Delete(data);
            return;
        }
        err = app_ha_call_entity_service_data("light", "turn_on",
                                               instance->entity_id, data);
        cJSON_Delete(data);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Brightness service for %s failed: %s",
                 instance->entity_id, esp_err_to_name(err));
    }
}

static esp_err_t light_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config ||
        instance->config->type != SCREEN_FEATURE_LIGHT_BRIGHTNESS) {
        return ESP_ERR_INVALID_ARG;
    }
    instance->control = ha_control_slider_create(instance->host);
    if (!instance->control) {
        return ESP_ERR_NO_MEM;
    }
    ha_control_slider_set_disabled(instance->control, true);
    ha_control_slider_set_change_cb(instance->control, brightness_changed,
                                    instance);
    return ESP_OK;
}

static void light_feature_update(entity_feature_instance_t *instance,
                                 ha_entity_store_t *store,
                                 const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) {
        return;
    }
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    double brightness = 0;
    uint8_t percent = 0;
    if (available && strcmp(snapshot->state, "on") == 0 &&
        ha_entity_store_get_attr_number(store, instance->entity_id,
                                        "brightness", &brightness) == ESP_OK &&
        isfinite(brightness)) {
        percent = (uint8_t)LV_CLAMP(0, (int)lround(brightness * 100.0 / 255.0), 100);
    }
    ha_control_slider_set_value(instance->control, percent, LV_ANIM_OFF);
    if (store) {
        ha_control_slider_set_active_color(
            instance->control,
            light_entity_active_color(store, instance->entity_id));
    }
    ha_control_slider_set_disabled(instance->control, !available);
}

const entity_feature_handler_t light_feature_handler = {
    .supports = light_feature_supports,
    .create = light_feature_create,
    .update = light_feature_update,
};
