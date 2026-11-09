#include "entity_handlers/entity_state_translation.h"
#include "i18n.h"
#include "climate_entity_handler.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "assets/mdi_symbol_def.h"
#include "climate_more_info.h"
#include "climate_feature_handler.h"
#include "esp_log.h"

static const char *TAG = "climate_entity_handler";

static uint32_t climate_mode_color(const char *mode)
{
    if (strcmp(mode, "heat") == 0) return 0xFF6F22;
    if (strcmp(mode, "cool") == 0) return 0x2196F3;
    if (strcmp(mode, "heat_cool") == 0) return 0xFFC107;
    if (strcmp(mode, "auto") == 0) return 0x4CAF50;
    if (strcmp(mode, "dry") == 0) return 0xFF9800;
    if (strcmp(mode, "fan_only") == 0) return 0x00BCD4;
    return 0x6E6E6E;
}

static const char *climate_hvac_mode_label(const char *state)
{
    return entity_state_translate_climate_option(0, state);
}

static const char *climate_action_badge(const char *action,
                                        uint32_t *color)
{
    /* Match Home Assistant's tile badge action-to-mode color mapping. */
    if (strcmp(action, "cooling") == 0) {
        *color = 0x2196F3;
        return MDI_SYMBOL_SNOWFLAKE;
    }
    if (strcmp(action, "defrosting") == 0) {
        *color = 0xFF6F22;
        return MDI_SYMBOL_SNOWFLAKE_MELT;
    }
    if (strcmp(action, "drying") == 0) {
        *color = 0xFF9800;
        return MDI_SYMBOL_WATER_PERCENT;
    }
    if (strcmp(action, "fan") == 0) {
        *color = 0x00BCD4;
        return MDI_SYMBOL_FAN;
    }
    if (strcmp(action, "heating") == 0) {
        *color = 0xFF6F22;
        return MDI_SYMBOL_FIRE;
    }
    if (strcmp(action, "idle") == 0) {
        *color = 0x9E9E9E;
        return MDI_SYMBOL_CLOCK_OUTLINE;
    }
    if (strcmp(action, "preheating") == 0) {
        *color = 0xFF6F22;
        return MDI_SYMBOL_HEAT_WAVE;
    }

    /* Home Assistant falls back to circle-medium and the entity state color. */
    return MDI_SYMBOL_CIRCLE_MEDIUM;
}

static bool climate_handle_action(const entity_action_context_t *context)
{
    if (entity_handler_handle_more_info_action(context)) {
        return true;
    }

    if (!context || !context->entity_id || !context->entity_id[0]) {
        return false;
    }

    const screen_action_t *action = entity_handler_get_action(context);
    if (!action || action->action != SCREEN_ACTION_TOGGLE) {
        return false;
    }

    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot;
    if (!store || ha_entity_store_get(store, context->entity_id, &snapshot) != ESP_OK) {
        ESP_LOGW(TAG, "Cannot toggle %s: entity state unavailable", context->entity_id);
        return true;
    }

    const char *service = strcmp(snapshot.state, "off") == 0 ? "turn_on" : "turn_off";
    esp_err_t err = app_ha_call_entity_service("climate", service, context->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "climate.%s for %s failed to send: %s",
                 service, context->entity_id, esp_err_to_name(err));
    }
    return true;
}

static esp_err_t climate_render_tile(ha_entity_store_t *store,
                                     const ha_entity_snapshot_t *snapshot,
                                     entity_display_model_t *model)
{
    if (!store || !snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = MDI_SYMBOL_THERMOSTAT;
    model->background_color = lv_color_hex(0x1E2228);
    model->icon_color = lv_color_hex(0x44739e);
    model->icon_circle_background_color = model->icon_color;
    model->icon_circle_background_color_light = model->icon_color;
    model->icon_circle_background_opa = LV_OPA_20;
    model->title_color = lv_color_hex(0xFFFFFF);
    model->state_color = lv_color_hex(0x666666);

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon_color = lv_color_hex(0x6f6f6f);
        model->icon_circle_background_color = model->icon_color;
        model->icon_circle_background_color_light = lv_color_hex(0xCDD0D6);
        return ESP_OK;
    }

    if (strcmp(snapshot->state, "off") == 0) {
        model->icon_color = lv_color_hex(0x6E6E6E);
        model->icon_circle_background_color = model->icon_color;
        model->icon_circle_background_color_light = model->icon_color;
    } else {
        model->icon_color = lv_color_hex(climate_mode_color(snapshot->state));
        model->icon_circle_background_color = model->icon_color;
        model->icon_circle_background_color_light = model->icon_color;
    }

    char hvac_action[HA_ENTITY_STORE_STATE_MAX] = "";
    if (ha_entity_store_get_attr_string(store, snapshot->entity_id,
                                        "hvac_action", hvac_action,
                                        sizeof(hvac_action)) == ESP_OK &&
        hvac_action[0] != '\0' && strcmp(hvac_action, "off") != 0) {
        uint32_t badge_color = strcmp(snapshot->state, "off") == 0
                                   ? 0x9E9E9E
                                   : climate_mode_color(snapshot->state);
        model->icon_badge = climate_action_badge(hvac_action, &badge_color);
        model->icon_badge_background_color = lv_color_hex(badge_color);
    }

    const char *mode = climate_hvac_mode_label(snapshot->state);
    if (mode) {
        snprintf(model->state_text, sizeof(model->state_text), "%s", mode);
    } else {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
    }

    double current_temperature = 0;
    if (ha_entity_store_get_attr_number(store,
                                        snapshot->entity_id,
                                        "current_temperature",
                                        &current_temperature) == ESP_OK &&
        isfinite(current_temperature)) {
        size_t length = strlen(model->state_text);
        snprintf(model->state_text + length,
                 sizeof(model->state_text) - length,
                 " · %.1f °C",
                 current_temperature);
    }

    return ESP_OK;
}

const entity_handler_t climate_entity_handler = {
    .domain = "climate",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_THERMOSTAT,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_NOTHING },
    },
    .handle_action = climate_handle_action,
    .render_tile = climate_render_tile,
    .render_more_info = climate_more_info_render,
    .update_more_info = climate_more_info_update,
    .feature_handler = &climate_feature_handler,
};
