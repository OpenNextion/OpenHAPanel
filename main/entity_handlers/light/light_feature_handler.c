#include "light_feature_handler.h"

#include <math.h>
#include <string.h>

#include "app.h"
#include "assets/mdi_symbol_def.h"
#include "common/ha_control_select_menu/ha_control_select_menu.h"
#include "common/ha_control_slider/ha_control_slider.h"
#include "common/ha_control_toggle/ha_control_toggle.h"
#include "esp_log.h"
#include "i18n.h"
#include "light_entity_handler.h"
#include "components/light_preset_colors.h"

static const char *TAG = "light_feature_handler";

#define LIGHT_TOGGLE_AMBER      0xFFC107
#define LIGHT_TOGGLE_OFF_COLOR  0x9E9E9E
#define LIGHT_COLOR_TEMP_DEFAULT_MIN_KELVIN 2700
#define LIGHT_COLOR_TEMP_DEFAULT_MAX_KELVIN 6500


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

static bool mode_supports_color_temp(const char *mode)
{
    return mode && strcmp(mode, "color_temp") == 0;
}

static bool light_feature_supports(ha_entity_store_t *store,
                                   const char *entity_id,
                                   screen_feature_type_t type)
{
    if (type == SCREEN_FEATURE_TOGGLE) {
        return true;
    }
    if (type == SCREEN_FEATURE_LIGHT_COLOR_FAVORITES) {
        light_favorite_color_t colors[LIGHT_FAVORITE_COLOR_MAX];
        return store && entity_id &&
            light_entity_favorite_palette(store, entity_id, colors) > 0;
    }
    if (type == SCREEN_FEATURE_LIGHT_EFFECT) {
        if (!store || !entity_id) {
            return true; /* Show configured control until capability data arrives. */
        }
        cJSON *effects = NULL;
        if (ha_entity_store_dup_attr_json(store, entity_id,
                                          "effect_list", &effects) != ESP_OK) {
            return true;
        }
        bool supported = false;
        const cJSON *effect = NULL;
        if (cJSON_IsArray(effects)) {
            cJSON_ArrayForEach(effect, effects) {
                if (cJSON_IsString(effect) && effect->valuestring &&
                    effect->valuestring[0] &&
                    strlen(effect->valuestring) < HA_CONTROL_SELECT_MENU_MAX_VALUE_LEN) {
                    supported = true;
                    break;
                }
            }
        }
        cJSON_Delete(effects);
        return supported;
    }
    if (type != SCREEN_FEATURE_LIGHT_BRIGHTNESS &&
        type != SCREEN_FEATURE_LIGHT_COLOR_TEMP) {
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
        if (!cJSON_IsString(mode)) {
            continue;
        }
        const bool match =
            (type == SCREEN_FEATURE_LIGHT_BRIGHTNESS &&
             mode_supports_brightness(mode->valuestring)) ||
            (type == SCREEN_FEATURE_LIGHT_COLOR_TEMP &&
             mode_supports_color_temp(mode->valuestring));
        if (match) {
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

static void color_temp_changed(uint8_t percent,
                               ha_control_slider_phase_t phase,
                               void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id || phase != HA_CONTROL_SLIDER_COMMIT) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store ||
        ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot) ||
        !light_feature_supports(store, instance->entity_id,
                                SCREEN_FEATURE_LIGHT_COLOR_TEMP)) {
        return;
    }
    double min_value = LIGHT_COLOR_TEMP_DEFAULT_MIN_KELVIN;
    double max_value = LIGHT_COLOR_TEMP_DEFAULT_MAX_KELVIN;
    double number = 0;
    if (ha_entity_store_get_attr_number(store, instance->entity_id,
                                        "min_color_temp_kelvin", &number) == ESP_OK &&
        isfinite(number)) {
        min_value = number;
    }
    if (ha_entity_store_get_attr_number(store, instance->entity_id,
                                        "max_color_temp_kelvin", &number) == ESP_OK &&
        isfinite(number)) {
        max_value = number;
    }
    if (max_value < min_value) {
        max_value = min_value;
    }
    const uint16_t kelvin = (uint16_t)lroundf(
        (float)min_value + (float)(max_value - min_value) * percent / 100.0f);

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return;
    }
    if (!cJSON_AddNumberToObject(data, "color_temp_kelvin", kelvin)) {
        cJSON_Delete(data);
        return;
    }
    const esp_err_t err = app_ha_call_entity_service_data(
        "light", "turn_on", instance->entity_id, data);
    cJSON_Delete(data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Color temperature service for %s failed: %s",
                 instance->entity_id, esp_err_to_name(err));
    }
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
    esp_err_t err = app_ha_call_entity_service("light", service,
                                                instance->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "light.%s for %s failed: %s",
                 service, instance->entity_id, esp_err_to_name(err));
    }
}

static void light_send_effect(entity_feature_instance_t *instance,
                              const char *value)
{
    if (!instance || !instance->entity_id || !instance->control ||
        !value || !value[0]) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store ||
        ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) {
        return;
    }
    cJSON *effects = NULL;
    if (ha_entity_store_dup_attr_json(store, instance->entity_id,
                                      "effect_list", &effects) != ESP_OK) {
        return;
    }
    bool valid = false;
    const cJSON *effect = NULL;
    if (cJSON_IsArray(effects)) {
        cJSON_ArrayForEach(effect, effects) {
            if (cJSON_IsString(effect) && effect->valuestring &&
                strcmp(effect->valuestring, value) == 0) {
                valid = true;
                break;
            }
        }
    }
    cJSON_Delete(effects);
    if (!valid) {
        return;
    }
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return;
    }
    if (!cJSON_AddStringToObject(data, "effect", value)) {
        cJSON_Delete(data);
        return;
    }
    const esp_err_t err = app_ha_call_entity_service_data(
        "light", "turn_on", instance->entity_id, data);
    cJSON_Delete(data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "light.turn_on (effect) for %s failed: %s",
                 instance->entity_id, esp_err_to_name(err));
    }
}

static void light_effect_changed(const char *value, void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->control) {
        return;
    }
    light_send_effect(instance, value);
    /* Update the trigger-button icon immediately on selection. */
    if (value && value[0]) {
        ha_control_select_menu_set_icon(instance->control,
                                        MDI_SYMBOL_CIRCLE_MEDIUM);
    }
}

static void light_favorite_changed(size_t index,
                                   const light_favorite_color_t *color,
                                   void *user_data)
{
    (void)index;
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id || !color) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store ||
        ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) {
        return;
    }
    light_send_preset_color(instance->entity_id, color);
}

static esp_err_t light_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (instance->config->type) {
    case SCREEN_FEATURE_LIGHT_BRIGHTNESS:
        instance->control = ha_control_slider_create(instance->host);
        if (!instance->control) {
            return ESP_ERR_NO_MEM;
        }
        ha_control_slider_set_disabled(instance->control, true);
        ha_control_slider_set_change_cb(instance->control, brightness_changed,
                                        instance);
        return ESP_OK;
    case SCREEN_FEATURE_LIGHT_COLOR_TEMP:
        instance->control = ha_control_slider_create(instance->host);
        if (!instance->control) {
            return ESP_ERR_NO_MEM;
        }
        ha_control_slider_set_disabled(instance->control, true);
        ha_control_slider_set_change_cb(instance->control, color_temp_changed,
                                        instance);
        return ESP_OK;
    case SCREEN_FEATURE_LIGHT_EFFECT:
        instance->control = ha_control_select_menu_create(instance->host);
        if (!instance->control) {
            return ESP_ERR_NO_MEM;
        }
        ha_control_select_menu_set_title(instance->control,
                                         i18n_get(I18N_LIGHT_EFFECT));
        ha_control_select_menu_set_icon(instance->control,
                                        MDI_SYMBOL_CREATION);
        ha_control_select_menu_set_disabled(instance->control, true);
        ha_control_select_menu_set_change_cb(instance->control,
                                             light_effect_changed, instance);
        return ESP_OK;
    case SCREEN_FEATURE_LIGHT_COLOR_FAVORITES:
        instance->control = light_preset_colors_create(instance->host);
        if (!instance->control) {
            return ESP_ERR_NO_MEM;
        }
        light_preset_colors_set_tile_layout(instance->control,
                                            instance->visible_limit);
        light_preset_colors_set_disabled(instance->control, true);
        light_preset_colors_set_select_cb(instance->control,
                                          light_favorite_changed, instance);
        return ESP_OK;
    case SCREEN_FEATURE_TOGGLE:
        instance->control = ha_control_toggle_create(instance->host);
        if (!instance->control) {
            return ESP_ERR_NO_MEM;
        }
        ha_control_toggle_set_interactive(instance->control, false);
        ha_control_toggle_set_change_cb(instance->control, toggle_changed,
                                        instance);
        ha_control_toggle_set_icon_text(instance->control,
                                        MDI_SYMBOL_LIGHTBULB_OFF);
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static void update_brightness(entity_feature_instance_t *instance,
                              ha_entity_store_t *store,
                              const ha_entity_snapshot_t *snapshot)
{
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

static void update_color_temp(entity_feature_instance_t *instance,
                              ha_entity_store_t *store,
                              const ha_entity_snapshot_t *snapshot)
{
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool on = available && strcmp(snapshot->state, "on") == 0;
    double min_value = LIGHT_COLOR_TEMP_DEFAULT_MIN_KELVIN;
    double max_value = LIGHT_COLOR_TEMP_DEFAULT_MAX_KELVIN;
    double kelvin = min_value;
    double number = 0;
    if (store) {
        if (ha_entity_store_get_attr_number(store, instance->entity_id,
                                            "min_color_temp_kelvin", &number) == ESP_OK &&
            isfinite(number)) {
            min_value = number;
        }
        if (ha_entity_store_get_attr_number(store, instance->entity_id,
                                            "max_color_temp_kelvin", &number) == ESP_OK &&
            isfinite(number)) {
            max_value = number;
        }
        if (ha_entity_store_get_attr_number(store, instance->entity_id,
                                            "color_temp_kelvin", &number) == ESP_OK &&
            isfinite(number)) {
            kelvin = number;
        }
    }
    if (max_value < min_value) {
        max_value = min_value;
    }
    kelvin = LV_CLAMP(min_value, kelvin, max_value);
    ha_control_slider_set_color_temp_range(instance->control,
                                           (uint16_t)min_value,
                                           (uint16_t)max_value);

    const float span = (float)(max_value - min_value);
    uint8_t percent = 0;
    if (on && span > 0.0f) {
        percent = (uint8_t)LV_CLAMP(
            0, (int)lroundf((float)(kelvin - min_value) * 100.0f / span), 100);
    }
    ha_control_slider_set_value(instance->control, percent, LV_ANIM_OFF);
    ha_control_slider_set_power(instance->control, on);
    ha_control_slider_set_disabled(instance->control, !available);
}

static void update_toggle(entity_feature_instance_t *instance,
                          ha_entity_store_t *store,
                          const ha_entity_snapshot_t *snapshot)
{
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool on = available && strcmp(snapshot->state, "on") == 0;

    const lv_color_t accent = store
        ? light_entity_toggle_color(store, instance->entity_id)
        : lv_color_hex(LIGHT_TOGGLE_AMBER);
    /* Colored lights keep their color on the knob while unavailable. */
    const lv_color_t off_knob =
        (!available && store &&
         light_entity_has_rgb_color(store, instance->entity_id))
        ? accent
        : lv_color_hex(LIGHT_TOGGLE_OFF_COLOR);
    const lv_color_t track_light =
        lv_color_mix(accent, lv_color_white(), LV_OPA_20);
    const lv_color_t track_off =
        lv_color_hex(0xE3E3E3);
    const lv_color_t track_off_dark =
        lv_color_hex(LIGHT_TOGGLE_OFF_COLOR);

    ha_control_toggle_set_active_color(instance->control, accent);
    ha_control_toggle_set_dark_off_colors(instance->control, track_off_dark,
                                          off_knob);
    ha_control_toggle_set_light_colors(instance->control,
                                       track_off, track_light,
                                       off_knob, accent,
                                       track_off, off_knob);
    ha_control_toggle_set_value(instance->control, on, LV_ANIM_ON);
    ha_control_toggle_set_interactive(instance->control, available);
    lv_obj_set_style_opa(instance->control,
                         available ? LV_OPA_COVER : LV_OPA_50, 0);
    ha_control_toggle_set_icon_text(instance->control,
                                    on ? MDI_SYMBOL_LIGHTBULB_ON
                                       : MDI_SYMBOL_LIGHTBULB_OFF);
}

static void light_apply_effect_availability_style(lv_obj_t *control,
                                                  bool available)
{
    if (!control) return;

    for (int light = 0; light <= 1; light++) {
        ha_control_select_menu_colors_t colors;
        if (!ha_control_select_menu_get_colors(control, light != 0, &colors)) {
            continue;
        }
        colors.button_color = light ? 0xE3E3E3 : 0x242424;
        colors.button_opa = LV_OPA_COVER;
        colors.text_color = available
                                ? (light ? 0x333333 : 0xFFFFFF)
                                : (light ? 0xBDBDBD : 0x464646);
        ha_control_select_menu_set_colors(control, light != 0, &colors);
    }
}

static void update_effect(entity_feature_instance_t *instance,
                          ha_entity_store_t *store,
                          const ha_entity_snapshot_t *snapshot)
{
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    cJSON *effects = NULL;
    if (store &&
        ha_entity_store_dup_attr_json(store, instance->entity_id,
                                      "effect_list", &effects) == ESP_OK &&
        cJSON_IsArray(effects)) {
        ha_control_select_menu_option_t
            options[HA_CONTROL_SELECT_MENU_MAX_OPTIONS] = {0};
        uint32_t count = 0;
        const cJSON *effect = NULL;
        cJSON_ArrayForEach(effect, effects) {
            if (!cJSON_IsString(effect) || !effect->valuestring ||
                !effect->valuestring[0] ||
                strlen(effect->valuestring) >=
                    HA_CONTROL_SELECT_MENU_MAX_VALUE_LEN) {
                continue;
            }
            if (count == HA_CONTROL_SELECT_MENU_MAX_OPTIONS) {
                break;
            }
            options[count] = (ha_control_select_menu_option_t){
                .value = effect->valuestring,
                .label = effect->valuestring,
                .symbol = MDI_SYMBOL_CIRCLE_MEDIUM,
            };
            count++;
        }
        ha_control_select_menu_set_options(instance->control, options, count);
        cJSON_Delete(effects);
    } else {
        cJSON_Delete(effects);
        ha_control_select_menu_set_options(instance->control, NULL, 0);
    }
    char selected[HA_CONTROL_SELECT_MENU_MAX_VALUE_LEN] = "";
    ha_entity_store_get_attr_string(store, instance->entity_id,
                                    "effect", selected, sizeof(selected));
    ha_control_select_menu_set_value(instance->control, selected);
    /* Match the trigger-button icon to the selected effect's option icon;
     * fall back to the title icon when nothing is selected. */
    const char *active = ha_control_select_menu_get_value(instance->control);
    ha_control_select_menu_set_icon(
        instance->control,
        active && active[0] ? MDI_SYMBOL_CIRCLE_MEDIUM
                            : MDI_SYMBOL_CREATION);
    ha_control_select_menu_set_disabled(
        instance->control,
        !available ||
            ha_control_select_menu_get_option_count(instance->control) == 0);
    light_apply_effect_availability_style(instance->control, available);
}

static void update_favorites(entity_feature_instance_t *instance,
                             ha_entity_store_t *store,
                             const ha_entity_snapshot_t *snapshot)
{
    light_favorite_color_t colors[LIGHT_FAVORITE_COLOR_MAX];
    const size_t count = light_entity_favorite_palette(store,
                                                        instance->entity_id,
                                                        colors);
    light_preset_colors_set_tile_layout(instance->control,
                                        instance->visible_limit);
    light_preset_colors_set_palette(instance->control, colors, count);
    light_preset_colors_set_disabled(instance->control,
        !store || !snapshot || entity_handler_snapshot_is_unavailable(snapshot));
}

static void light_feature_update(entity_feature_instance_t *instance,
                                 ha_entity_store_t *store,
                                 const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control || !instance->config) {
        return;
    }
    switch (instance->config->type) {
    case SCREEN_FEATURE_LIGHT_BRIGHTNESS:
        update_brightness(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_LIGHT_COLOR_TEMP:
        update_color_temp(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_LIGHT_EFFECT:
        update_effect(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_LIGHT_COLOR_FAVORITES:
        update_favorites(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_TOGGLE:
        update_toggle(instance, store, snapshot);
        break;
    default:
        break;
    }
}

static void light_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (!instance || !instance->control || !instance->config) {
        return;
    }
    switch (instance->config->type) {
    case SCREEN_FEATURE_TOGGLE:
        ha_control_toggle_refresh_theme(instance->control);
        break;
    case SCREEN_FEATURE_LIGHT_BRIGHTNESS:
    case SCREEN_FEATURE_LIGHT_COLOR_TEMP:
        ha_control_slider_refresh_theme(instance->control);
        break;
    case SCREEN_FEATURE_LIGHT_EFFECT:
        ha_control_select_menu_apply_theme(instance->control);
        break;
    case SCREEN_FEATURE_LIGHT_COLOR_FAVORITES:
        light_preset_colors_refresh_theme(instance->control);
        break;
    default:
        break;
    }
}

const entity_feature_handler_t light_feature_handler = {
    .supports = light_feature_supports,
    .create = light_feature_create,
    .update = light_feature_update,
    .apply_theme = light_feature_apply_theme,
};
