#include "climate_feature_handler.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "common/ha_control_select/ha_control_select.h"
#include "common/ha_control_select_menu/ha_control_select_menu.h"
#include "common/ha_control_slider/ha_control_slider.h"
#include "components/climate_target_temperature.h"
#include "entity_handlers/entity_state_translation.h"
#include "esp_log.h"
#include "i18n.h"
#include "screen_theme.h"

#define CLIMATE_SELECT_MAX_OPTIONS 16
#define CLIMATE_SLIDER_OFF_COLOR         0x9E9E9E
#define CLIMATE_SLIDER_UNAVAILABLE_COLOR 0x6F6F6F
#define CLIMATE_SELECT_SURFACE_DARK      0x242424
#define CLIMATE_SELECT_BUTTON_LIGHT      0xE3E3E3
#define CLIMATE_SELECT_TEXT_LIGHT        0x333333
#define CLIMATE_SELECT_TEXT_DARK         0xFFFFFF
#define CLIMATE_SELECT_DISABLED_LIGHT    0xBDBDBD
#define CLIMATE_SELECT_DISABLED_DARK     0x464646
#define CLIMATE_TARGET_TEMPERATURE       (1U << 0)
#define CLIMATE_TARGET_TEMPERATURE_RANGE (1U << 1)

static const char *TAG = "climate_feature_handler";

typedef struct {
    screen_feature_type_t type;
    unsigned selector;
    const char *options_attribute;
    const char *value_attribute;
    const char *service;
    const char *service_key;
} climate_select_feature_t;

static const climate_select_feature_t s_select_features[] = {
    {SCREEN_FEATURE_CLIMATE_HVAC_MODES, 0, "hvac_modes", NULL, "set_hvac_mode", "hvac_mode"},
    {SCREEN_FEATURE_CLIMATE_PRESET_MODES, 1, "preset_modes", "preset_mode", "set_preset_mode", "preset_mode"},
    {SCREEN_FEATURE_CLIMATE_FAN_MODES, 2, "fan_modes", "fan_mode", "set_fan_mode", "fan_mode"},
    {SCREEN_FEATURE_CLIMATE_SWING_MODES, 3, "swing_modes", "swing_mode", "set_swing_mode", "swing_mode"},
    {SCREEN_FEATURE_CLIMATE_SWING_HORIZONTAL_MODES, 4, "swing_horizontal_modes", "swing_horizontal_mode", "set_swing_horizontal_mode", "swing_horizontal_mode"},
};

static const climate_select_feature_t *select_feature(screen_feature_type_t type)
{
    for (size_t i = 0; i < sizeof(s_select_features) / sizeof(s_select_features[0]); i++) {
        if (s_select_features[i].type == type) return &s_select_features[i];
    }
    return NULL;
}

static bool climate_feature_supports_config(const screen_feature_config_t *config)
{
    return config &&
           (select_feature(config->type) ||
            config->type == SCREEN_FEATURE_CLIMATE_TARGET_TEMPERATURE ||
            config->type == SCREEN_FEATURE_CLIMATE_TARGET_HUMIDITY);
}

typedef struct {
    climate_temperature_target_t targets[2];
    size_t count;
    char unit[12];
    bool has_data;
} climate_temperature_state_t;

static double temperature_clamp(double value, double min, double max)
{
    return fmin(max, fmax(min, value));
}

static bool climate_temperature_read(ha_entity_store_t *store,
                                     const char *entity_id,
                                     climate_temperature_state_t *state)
{
    if (!store || !entity_id || !state) return false;
    memset(state, 0, sizeof(*state));
    double min = 7, max = 35, step = 0.5, features = 0;
    double target, low, high;
    ha_entity_store_get_attr_number(store, entity_id, "min_temp", &min);
    ha_entity_store_get_attr_number(store, entity_id, "max_temp", &max);
    ha_entity_store_get_attr_number(store, entity_id, "target_temp_step", &step);
    ha_entity_store_get_attr_number(store, entity_id, "supported_features", &features);
    if (!isfinite(min) || !isfinite(max) || !isfinite(step) ||
        !isfinite(features)) return false;
    max = fmax(min, max);
    step = fmax(0.1, step);
    if (ha_entity_store_get_attr_string(store, entity_id, "temperature_unit",
                                        state->unit, sizeof(state->unit)) != ESP_OK ||
        !state->unit[0]) {
        lv_strlcpy(state->unit, "°C", sizeof(state->unit));
    }
    const uint32_t flags = features >= 0 && features <= UINT32_MAX
        ? (uint32_t)features : 0;
    const bool single_capable = (flags & CLIMATE_TARGET_TEMPERATURE) != 0;
    const bool range_capable =
        (flags & CLIMATE_TARGET_TEMPERATURE_RANGE) != 0;
    const bool has_target =
        ha_entity_store_get_attr_number(store, entity_id, "temperature", &target) == ESP_OK &&
        isfinite(target);
    const bool has_low =
        ha_entity_store_get_attr_number(store, entity_id, "target_temp_low", &low) == ESP_OK &&
        isfinite(low);
    const bool has_high =
        ha_entity_store_get_attr_number(store, entity_id, "target_temp_high", &high) == ESP_OK &&
        isfinite(high);
    const bool has_range_data = has_low && low > 0 && has_high && high > 0;

    if (single_capable && has_target) {
        state->targets[0] = (climate_temperature_target_t){
            .value = temperature_clamp(target, min, max),
            .min = min, .max = max, .step = step
        };
        state->count = 1;
        state->has_data = true;
        return true;
    }
    if (range_capable && has_range_data) {
        low = temperature_clamp(low, min, max);
        high = temperature_clamp(high, min, max);
        if (low > high) low = high;
        state->targets[0] = (climate_temperature_target_t){
            .value = low, .min = min, .max = high, .step = step
        };
        state->targets[1] = (climate_temperature_target_t){
            .value = high, .min = low, .max = max, .step = step
        };
        state->count = 2;
        state->has_data = true;
        return true;
    }
    if (range_capable || single_capable) {
        /* No usable target data yet: render a single, disabled control. */
        state->targets[0] = (climate_temperature_target_t){
            .value = min, .min = min, .max = max, .step = step
        };
        state->count = 1;
        state->has_data = false;
        return true;
    }
    if (has_range_data) {
        low = temperature_clamp(low, min, max);
        high = temperature_clamp(high, min, max);
        if (low > high) low = high;
        state->targets[0] = (climate_temperature_target_t){
            .value = low, .min = min, .max = high, .step = step
        };
        state->targets[1] = (climate_temperature_target_t){
            .value = high, .min = low, .max = max, .step = step
        };
        state->count = 2;
        state->has_data = true;
        return true;
    }
    return false;
}

static bool climate_feature_is_slider(screen_feature_type_t type)
{
    return type == SCREEN_FEATURE_CLIMATE_TARGET_HUMIDITY;
}

static bool climate_feature_is_dropdown(const screen_feature_config_t *config)
{
    if (!config || climate_feature_is_slider(config->type)) return false;
    const cJSON *style = config->data
        ? cJSON_GetObjectItemCaseSensitive(config->data, "style") : NULL;
    if (cJSON_IsString(style) && style->valuestring && style->valuestring[0]) {
        return strcmp(style->valuestring, "dropdown") == 0;
    }
    /* Fallback defaults when the frontend did not pass a style. */
    switch (config->type) {
    case SCREEN_FEATURE_CLIMATE_FAN_MODES:
    case SCREEN_FEATURE_CLIMATE_PRESET_MODES:
    case SCREEN_FEATURE_CLIMATE_SWING_MODES:
    case SCREEN_FEATURE_CLIMATE_SWING_HORIZONTAL_MODES:
        return true;
    default:
        return false; /* Climate HVAC modes use the icons style. */
    }
}

static bool climate_light_theme(void)
{
    return screen_theme_get() == SCREEN_THEME_LIGHT;
}

static void climate_select_apply_surface(lv_obj_t *control)
{
    const bool light = climate_light_theme();
    ha_control_select_set_surface_style(
        control,
        lv_color_hex(light ? 0x999999 : CLIMATE_SELECT_SURFACE_DARK),
        light ? 71 : LV_OPA_COVER);
}

static void climate_select_apply_text_color(lv_obj_t *control)
{
    ha_control_select_set_text_color(control,
        lv_color_hex(climate_light_theme() ? 0x141414 : 0xFFFFFF));
}

static void climate_apply_dropdown_disabled_style(lv_obj_t *control,
                                                  bool available)
{
    if (!control) return;
    for (int light = 0; light <= 1; light++) {
        ha_control_select_menu_colors_t colors;
        if (!ha_control_select_menu_get_colors(control, light != 0, &colors)) {
            continue;
        }
        colors.button_color = light ? CLIMATE_SELECT_BUTTON_LIGHT
                                    : CLIMATE_SELECT_SURFACE_DARK;
        colors.button_opa = LV_OPA_COVER;
        colors.text_color = available
                                ? (light ? CLIMATE_SELECT_TEXT_LIGHT
                                         : CLIMATE_SELECT_TEXT_DARK)
                                : (light ? CLIMATE_SELECT_DISABLED_LIGHT
                                         : CLIMATE_SELECT_DISABLED_DARK);
        ha_control_select_menu_set_colors(control, light != 0, &colors);
    }
}

static void climate_apply_icons_disabled_style(lv_obj_t *control,
                                               bool available)
{
    if (!control) return;

    if (available) {
        climate_select_apply_surface(control);
        climate_select_apply_text_color(control);
        ha_control_select_set_disabled_opa(control, LV_OPA_40);
        lv_obj_set_style_opa(control, LV_OPA_COVER, LV_PART_MAIN);
        return;
    }

    const lv_color_t disabled = lv_color_hex(
        climate_light_theme() ? CLIMATE_SELECT_DISABLED_LIGHT
                              : CLIMATE_SELECT_DISABLED_DARK);
    ha_control_select_set_disabled_opa(control, LV_OPA_COVER);
    ha_control_select_set_surface_style(
        control,
        lv_color_hex(climate_light_theme() ? CLIMATE_SELECT_BUTTON_LIGHT
                                           : CLIMATE_SELECT_SURFACE_DARK),
        LV_OPA_COVER);
    ha_control_select_set_text_color(control, disabled);
    ha_control_select_set_color(control, disabled);
    lv_obj_set_style_opa(control, LV_OPA_COVER, LV_PART_MAIN);
}

static const char *climate_feature_icon(unsigned selector)
{
    static const char *icons[] = {
        MDI_SYMBOL_POWER, MDI_SYMBOL_TUNE_VARIANT, MDI_SYMBOL_FAN,
        MDI_SYMBOL_ARROW_OSCILLATING, MDI_SYMBOL_ARROW_LEFT_RIGHT
    };
    return selector < sizeof(icons) / sizeof(icons[0])
        ? icons[selector] : MDI_SYMBOL_CIRCLE_MEDIUM;
}

static const char *climate_option_icon(unsigned selector, const char *value)
{
    static const struct {
        unsigned selector;
        const char *mode;
        const char *icon;
    } icons[] = {
        {0, "off", MDI_SYMBOL_POWER},
        {0, "heat", MDI_SYMBOL_FIRE},
        {0, "cool", MDI_SYMBOL_SNOWFLAKE},
        {0, "heat_cool", MDI_SYMBOL_SUN_SNOWFLAKE_VARIANT},
        {0, "auto", MDI_SYMBOL_THERMOSTAT_AUTO},
        {0, "dry", MDI_SYMBOL_WATER_PERCENT},
        {0, "fan_only", MDI_SYMBOL_FAN},
        {1, "eco", MDI_SYMBOL_LEAF},
        {1, "away", MDI_SYMBOL_ACCOUNT_ARROW_RIGHT},
        {1, "home", MDI_SYMBOL_HOME},
        {1, "sleep", MDI_SYMBOL_BED},
        {1, "comfort", MDI_SYMBOL_SOFA},
        {1, "boost", MDI_SYMBOL_ROCKET_LAUNCH},
        {1, "activity", MDI_SYMBOL_MOTION_SENSOR},
        {2, "off", MDI_SYMBOL_FAN_OFF},
        {2, "on", MDI_SYMBOL_FAN},
        {2, "auto", MDI_SYMBOL_FAN_AUTO},
        {2, "low", MDI_SYMBOL_SPEEDOMETER_SLOW},
        {2, "medium", MDI_SYMBOL_SPEEDOMETER_MEDIUM},
        {2, "middle", MDI_SYMBOL_SPEEDOMETER_MEDIUM},
        {2, "high", MDI_SYMBOL_SPEEDOMETER},
        {2, "diffuse", MDI_SYMBOL_WEATHER_WINDY},
        {2, "focus", MDI_SYMBOL_TARGET},
        {2, "silent", MDI_SYMBOL_VOLUME_LOW},
        {3, "off", MDI_SYMBOL_ARROW_OSCILLATING_OFF},
        {3, "on", MDI_SYMBOL_ARROW_OSCILLATING},
        {3, "auto", MDI_SYMBOL_ARROW_OSCILLATING},
        {3, "vertical", MDI_SYMBOL_ARROW_UP_DOWN},
        {3, "horizontal", MDI_SYMBOL_ARROW_LEFT_RIGHT},
        {3, "both", MDI_SYMBOL_ARROW_ALL},
        {4, "off", MDI_SYMBOL_ARROW_OSCILLATING_OFF},
        {4, "on", MDI_SYMBOL_ARROW_OSCILLATING},
        {4, "auto", MDI_SYMBOL_ARROW_OSCILLATING},
        {4, "left", MDI_SYMBOL_ARROW_LEFT},
        {4, "center", MDI_SYMBOL_FORMAT_HORIZONTAL_ALIGN_CENTER},
        {4, "right", MDI_SYMBOL_ARROW_RIGHT},
        {4, "both", MDI_SYMBOL_ARROW_LEFT_RIGHT},
        {4, "wide", MDI_SYMBOL_ARROW_LEFT_RIGHT},
    };
    char mode[48];
    while (*value == ' ') value++;
    size_t length = strlen(value);
    while (length && value[length - 1] == ' ') length--;
    length = LV_MIN(length, sizeof(mode) - 1);
    for (size_t i = 0; i < length; i++) {
        mode[i] = (char)tolower((unsigned char)value[i]);
    }
    mode[length] = '\0';
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
        if (icons[i].selector == selector &&
            strcmp(icons[i].mode, mode) == 0) return icons[i].icon;
    }
    return MDI_SYMBOL_CIRCLE_MEDIUM;
}

static int climate_hvac_mode_priority(const char *mode)
{
    static const char *const order[] = {
        "off", "fan_only", "dry", "cool", "heat", "heat_cool", "auto"
    };
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (strcmp(mode, order[i]) == 0) {
            return (int)i;
        }
    }
    return (int)(sizeof(order) / sizeof(order[0]));
}

static void climate_sort_hvac_modes(char (*values)[HA_CONTROL_SELECT_MAX_VALUE_LEN],
                                    uint32_t count)
{
    for (uint32_t i = 1; i < count; i++) {
        char key[HA_CONTROL_SELECT_MAX_VALUE_LEN];
        lv_strlcpy(key, values[i], sizeof(key));
        uint32_t j = i;
        while (j > 0 && climate_hvac_mode_priority(values[j - 1]) >
                        climate_hvac_mode_priority(key)) {
            lv_strlcpy(values[j], values[j - 1], sizeof(values[j]));
            j--;
        }
        lv_strlcpy(values[j], key, sizeof(values[j]));
    }
}

static uint32_t hvac_mode_color(const char *mode)
{
    if (strcmp(mode, "heat") == 0) return 0xFF6F22;
    if (strcmp(mode, "cool") == 0) return 0x2196F3;
    if (strcmp(mode, "heat_cool") == 0) return 0xFFC107;
    if (strcmp(mode, "auto") == 0) return 0x4CAF50;
    if (strcmp(mode, "dry") == 0) return 0xFF9800;
    if (strcmp(mode, "fan_only") == 0) return 0x00BCD4;
    return 0x6E6E6E;
}

static bool climate_feature_supports(ha_entity_store_t *store,
                                     const char *entity_id,
                                     screen_feature_type_t type)
{
    if (type == SCREEN_FEATURE_CLIMATE_TARGET_TEMPERATURE) {
        if (!store || !entity_id) return true;
        double features;
        if (ha_entity_store_get_attr_number(store, entity_id,
                                            "supported_features", &features) != ESP_OK)
            return true; /* Keep the feature while attributes are loading. */
        climate_temperature_state_t state;
        return climate_temperature_read(store, entity_id, &state);
    }
    if (climate_feature_is_slider(type)) {
        /* Always render the humidity slider; with no humidity attribute it
         * simply rests at 0% and waits for the value to arrive. */
        return true;
    }

    const climate_select_feature_t *feature = select_feature(type);
    if (!feature) return false;
    if (!store || !entity_id) return true;

    cJSON *modes = NULL;
    if (ha_entity_store_dup_attr_json(store, entity_id,
                                      feature->options_attribute, &modes) != ESP_OK) {
        return true; /* Keep the configured feature until attributes arrive. */
    }
    bool supported = false;
    const cJSON *mode = NULL;
    if (cJSON_IsArray(modes)) {
        cJSON_ArrayForEach(mode, modes) {
            if (cJSON_IsString(mode) && mode->valuestring && mode->valuestring[0] &&
                strlen(mode->valuestring) < HA_CONTROL_SELECT_MAX_VALUE_LEN) {
                supported = true;
                break;
            }
        }
    }
    cJSON_Delete(modes);
    return supported;
}

static void climate_send_option(entity_feature_instance_t *instance, const char *value)
{
    if (!instance || !instance->entity_id || !instance->control || !instance->config) return;
    const climate_select_feature_t *feature = select_feature(instance->config->type);
    if (!feature) return;
    if (!value || !value[0]) return;

    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store || ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) return;

    cJSON *modes = NULL;
    if (ha_entity_store_dup_attr_json(store, instance->entity_id,
                                      feature->options_attribute, &modes) != ESP_OK) return;
    bool valid = false;
    const cJSON *mode = NULL;
    if (cJSON_IsArray(modes)) {
        cJSON_ArrayForEach(mode, modes) {
            if (cJSON_IsString(mode) && mode->valuestring &&
                strcmp(mode->valuestring, value) == 0) {
                valid = true;
                break;
            }
        }
    }
    cJSON_Delete(modes);
    if (!valid) return;

    cJSON *data = cJSON_CreateObject();
    if (!data) return;
    if (!cJSON_AddStringToObject(data, feature->service_key, value)) {
        cJSON_Delete(data);
        return;
    }
    esp_err_t err = app_ha_call_entity_service_data("climate", feature->service,
                                                    instance->entity_id, data);
    cJSON_Delete(data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "climate.%s for %s failed: %s",
                 feature->service, instance->entity_id, esp_err_to_name(err));
    }
}

static void climate_option_changed(lv_event_t *event)
{
    entity_feature_instance_t *instance = lv_event_get_user_data(event);
    if (!instance || !instance->control) return;
    climate_send_option(instance, ha_control_select_get_value(instance->control));
}

static void climate_menu_option_changed(const char *value, void *user_data)
{
    climate_send_option(user_data, value);
}

static uint32_t climate_humidity_fill_color(const ha_entity_snapshot_t *snapshot,
                                            bool available)
{
    if (!available) {
        return CLIMATE_SLIDER_UNAVAILABLE_COLOR;
    }
    const char *mode = snapshot ? snapshot->state : "off";
    if (!mode || strcmp(mode, "off") == 0) {
        return CLIMATE_SLIDER_OFF_COLOR;
    }
    return hvac_mode_color(mode);
}

static void climate_humidity_changed(uint8_t percent,
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
        !climate_feature_supports(store, instance->entity_id,
                                  SCREEN_FEATURE_CLIMATE_TARGET_HUMIDITY)) {
        return;
    }
    double min_value = 30, max_value = 99;
    ha_entity_store_get_attr_number(store, instance->entity_id,
                                    "min_humidity", &min_value);
    ha_entity_store_get_attr_number(store, instance->entity_id,
                                    "max_humidity", &max_value);
    if (max_value < min_value) {
        max_value = min_value;
    }
    const uint8_t humidity = (uint8_t)lroundf(
        (float)min_value + (float)(max_value - min_value) * percent / 100.0f);
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return;
    }
    if (!cJSON_AddNumberToObject(data, "humidity", (double)humidity)) {
        cJSON_Delete(data);
        return;
    }
    const esp_err_t err = app_ha_call_entity_service_data(
        "climate", "set_humidity", instance->entity_id, data);
    cJSON_Delete(data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "climate.set_humidity for %s failed: %s",
                 instance->entity_id, esp_err_to_name(err));
    }
}

static void stop_feature_event(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
}

static void climate_temperature_changed(size_t index, int direction,
                                        void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id || !instance->control) return;
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    climate_temperature_state_t state;
    if (!store ||
        ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot) ||
        !climate_temperature_read(store, instance->entity_id, &state) ||
        !state.has_data || index >= state.count) return;

    climate_temperature_target_t displayed[2];
    if (climate_target_temperature_get(instance->control, displayed) == state.count) {
        for (size_t i = 0; i < state.count; i++) {
            state.targets[i].value = temperature_clamp(displayed[i].value,
                                                        state.targets[i].min,
                                                        state.targets[i].max);
        }
        if (state.count == 2) {
            state.targets[0].max = state.targets[1].value;
            state.targets[1].min = state.targets[0].value;
        }
    }
    climate_temperature_target_t *target = &state.targets[index];
    double next = temperature_clamp(target->value + direction * target->step,
                                    target->min, target->max);
    next = round(next * 1000.0) / 1000.0;
    if (fabs(next - target->value) < 0.00001) return;
    target->value = next;
    cJSON *data = cJSON_CreateObject();
    if (!data) return;
    bool built;
    if (state.count == 1) {
        built = cJSON_AddNumberToObject(data, "temperature", next) != NULL;
    } else {
        built = cJSON_AddNumberToObject(data, "target_temp_low",
                                        state.targets[0].value) != NULL &&
                cJSON_AddNumberToObject(data, "target_temp_high",
                                        state.targets[1].value) != NULL;
    }
    if (built) {
        esp_err_t err = app_ha_call_entity_service_data(
            "climate", "set_temperature", instance->entity_id, data);
        if (err == ESP_OK) {
            if (state.count == 2) {
                state.targets[0].max = state.targets[1].value;
                state.targets[1].min = state.targets[0].value;
            }
            climate_target_temperature_set(instance->control, state.targets,
                                           state.count, state.unit, true,
                                           instance->half_width);
        } else {
            ESP_LOGW(TAG, "climate.set_temperature for %s failed: %s",
                     instance->entity_id, esp_err_to_name(err));
        }
    }
    cJSON_Delete(data);
}

static esp_err_t climate_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id || !instance->config ||
        !climate_feature_supports_config(instance->config)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (instance->config->type == SCREEN_FEATURE_CLIMATE_TARGET_TEMPERATURE) {
        instance->control = climate_target_temperature_create(instance->host);
        if (!instance->control) return ESP_ERR_NO_MEM;
        climate_target_temperature_set_change_cb(instance->control,
                                                  climate_temperature_changed,
                                                  instance);
        return ESP_OK;
    }
    if (climate_feature_is_slider(instance->config->type)) {
        instance->control = ha_control_slider_create(instance->host);
        if (!instance->control) return ESP_ERR_NO_MEM;
        ha_control_slider_set_linear_fill(instance->control, true);
        ha_control_slider_set_power(instance->control, true);
        ha_control_slider_set_disabled(instance->control, true);
        ha_control_slider_set_change_cb(instance->control,
                                          climate_humidity_changed, instance);
        return ESP_OK;
    }
    if (climate_feature_is_dropdown(instance->config)) {
        const climate_select_feature_t *feature = select_feature(instance->config->type);
        instance->control = ha_control_select_menu_create(instance->host);
        if (!instance->control) return ESP_ERR_NO_MEM;
        ha_control_select_menu_set_icon(instance->control,
                                         climate_feature_icon(feature->selector));
        ha_control_select_menu_set_disabled(instance->control, true);
        ha_control_select_menu_set_change_cb(instance->control,
                                              climate_menu_option_changed, instance);
    } else {
        instance->control = ha_control_select_create(instance->host);
        if (!instance->control) return ESP_ERR_NO_MEM;
        climate_select_apply_surface(instance->control);
        ha_control_select_set_height(instance->control, 56);
        ha_control_select_set_hide_option_label(instance->control, true);
        ha_control_select_set_symbol_font(instance->control, &ha_font_mdi_20);
        climate_select_apply_text_color(instance->control);
        ha_control_select_set_disabled(instance->control, true);
        lv_obj_remove_flag(instance->control, LV_OBJ_FLAG_SCROLLABLE |
                                              LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(instance->control, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(instance->control, stop_feature_event, LV_EVENT_ALL, NULL);
        lv_obj_add_event_cb(instance->control, climate_option_changed,
                            LV_EVENT_VALUE_CHANGED, instance);
    }
    return ESP_OK;
}

static void climate_feature_update(entity_feature_instance_t *instance,
                                   ha_entity_store_t *store,
                                   const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control || !instance->config) return;
    if (instance->config->type == SCREEN_FEATURE_CLIMATE_TARGET_TEMPERATURE) {
        climate_temperature_state_t state;
        const bool has_target = climate_temperature_read(store,
                                                         instance->entity_id,
                                                         &state);
        const bool available = has_target && state.has_data && snapshot &&
                               !entity_handler_snapshot_is_unavailable(snapshot);
        climate_target_temperature_set(instance->control,
                                       has_target ? state.targets : NULL,
                                       has_target ? state.count : 0,
                                       has_target ? state.unit : "°C", available,
                                       instance->half_width);
        return;
    }
    if (climate_feature_is_slider(instance->config->type)) {
        const bool available = store && snapshot &&
                               !entity_handler_snapshot_is_unavailable(snapshot);
        double min_value = 30, max_value = 99, humidity = min_value;
        if (store) {
            ha_entity_store_get_attr_number(store, instance->entity_id,
                                            "min_humidity", &min_value);
            ha_entity_store_get_attr_number(store, instance->entity_id,
                                            "max_humidity", &max_value);
            ha_entity_store_get_attr_number(store, instance->entity_id,
                                            "humidity", &humidity);
        }
        if (max_value < min_value) {
            max_value = min_value;
        }
        uint8_t percent = 0;
        if (available && isfinite(humidity)) {
            percent = (uint8_t)LV_CLAMP(
                0, (int)lroundf((float)(humidity - min_value) /
                                (float)(max_value - min_value) * 100.0f), 100);
        }
        ha_control_slider_set_value(instance->control, percent, LV_ANIM_OFF);
        const lv_color_t color =
            lv_color_hex(climate_humidity_fill_color(snapshot, available));
        ha_control_slider_set_active_color_raw(instance->control, color);
        ha_control_slider_set_off_track_style(instance->control, color,
                                              LV_OPA_20);
        ha_control_slider_set_disabled(instance->control, !available);
        return;
    }
    const climate_select_feature_t *feature = select_feature(instance->config->type);
    if (!feature) return;
    const bool dropdown = climate_feature_is_dropdown(instance->config);
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    cJSON *modes = NULL;
    if (store && ha_entity_store_dup_attr_json(store, instance->entity_id,
                                               feature->options_attribute, &modes) == ESP_OK &&
        cJSON_IsArray(modes)) {
        ha_control_select_option_t icon_options[CLIMATE_SELECT_MAX_OPTIONS] = {0};
        ha_control_select_menu_option_t menu_options[CLIMATE_SELECT_MAX_OPTIONS] = {0};
        char values[CLIMATE_SELECT_MAX_OPTIONS][HA_CONTROL_SELECT_MAX_VALUE_LEN] = {0};
        uint32_t count = 0;
        const cJSON *mode = NULL;
        cJSON_ArrayForEach(mode, modes) {
            if (!cJSON_IsString(mode) || !mode->valuestring || !mode->valuestring[0] ||
                strlen(mode->valuestring) >= HA_CONTROL_SELECT_MAX_VALUE_LEN) continue;
            if (count == CLIMATE_SELECT_MAX_OPTIONS) break;
            lv_strlcpy(values[count], mode->valuestring, sizeof(values[count]));
            count++;
        }
        if (feature->type == SCREEN_FEATURE_CLIMATE_HVAC_MODES) {
            climate_sort_hvac_modes(values, count);
        }
        for (uint32_t i = 0; i < count; i++) {
            const char *value = values[i];
            const char *label = entity_state_translate_climate_option(feature->selector, value);
            const char *symbol = climate_option_icon(feature->selector, value);
            if (dropdown) {
                menu_options[i] = (ha_control_select_menu_option_t){
                    .value = value, .label = label, .symbol = symbol
                };
            } else {
                icon_options[i] = (ha_control_select_option_t){
                    .value = value, .label = label, .symbol = symbol
                };
            }
        }
        if (dropdown) {
            ha_control_select_menu_set_options(instance->control, menu_options, count);
        } else {
            ha_control_select_set_options_array(instance->control, icon_options, count);
        }
        cJSON_Delete(modes);
    } else {
        cJSON_Delete(modes);
        if (dropdown) {
            ha_control_select_menu_set_options(instance->control, NULL, 0);
        } else {
            ha_control_select_set_options_array(instance->control, NULL, 0);
        }
    }
    char selected[HA_ENTITY_STORE_STATE_MAX] = "";
    if (feature->value_attribute) {
        ha_entity_store_get_attr_string(store, instance->entity_id,
                                        feature->value_attribute,
                                        selected, sizeof(selected));
    } else if (snapshot) {
        lv_strlcpy(selected, snapshot->state, sizeof(selected));
    }
    const lv_color_t color =
        lv_color_hex(hvac_mode_color(snapshot ? snapshot->state : "off"));
    if (dropdown) {
        if (feature->type == SCREEN_FEATURE_CLIMATE_HVAC_MODES) {
            ha_control_select_menu_set_icon(instance->control,
                                            available
                                                ? climate_feature_icon(feature->selector)
                                                : MDI_SYMBOL_THERMOSTAT);
        }
        ha_control_select_menu_set_value(instance->control, selected);
        ha_control_select_menu_set_disabled(instance->control, !available ||
                                            ha_control_select_menu_get_option_count(
                                                instance->control) == 0);
        climate_apply_dropdown_disabled_style(instance->control, available);
    } else {
        ha_control_select_set_value(instance->control, selected);
        ha_control_select_set_color(instance->control, color);
        ha_control_select_set_disabled(instance->control, !available ||
                                       ha_control_select_get_option_count(instance->control) == 0);
        climate_apply_icons_disabled_style(instance->control, available);
    }
}

static void climate_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (instance && instance->control && instance->config &&
        instance->config->type == SCREEN_FEATURE_CLIMATE_TARGET_TEMPERATURE) {
        climate_target_temperature_apply_theme(instance->control);
        return;
    }
    if (instance && instance->control && instance->config &&
        climate_feature_is_dropdown(instance->config)) {
        ha_control_select_menu_apply_theme(instance->control);
    }
    if (instance && instance->control && instance->config &&
        select_feature(instance->config->type) &&
        !climate_feature_is_dropdown(instance->config)) {
        climate_select_apply_surface(instance->control);
        climate_select_apply_text_color(instance->control);
        climate_apply_icons_disabled_style(
            instance->control,
            !ha_control_select_get_disabled(instance->control));
    }
}

const entity_feature_handler_t climate_feature_handler = {
    .supports = climate_feature_supports,
    .supports_config = climate_feature_supports_config,
    .create = climate_feature_create,
    .update = climate_feature_update,
    .apply_theme = climate_feature_apply_theme,
};
