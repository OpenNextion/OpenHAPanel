#include "climate_feature_handler.h"

#include <ctype.h>
#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "common/ha_control_select/ha_control_select.h"
#include "common/ha_control_select_menu/ha_control_select_menu.h"
#include "entity_handlers/entity_state_translation.h"
#include "esp_log.h"
#include "i18n.h"

#define CLIMATE_SELECT_MAX_OPTIONS 16

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
    return config && select_feature(config->type);
}

static bool climate_feature_is_dropdown(const screen_feature_config_t *config)
{
    const cJSON *style = config->data
        ? cJSON_GetObjectItemCaseSensitive(config->data, "style") : NULL;
    return cJSON_IsString(style) && style->valuestring &&
           strcmp(style->valuestring, "dropdown") == 0;
}

static const char *climate_feature_title(unsigned selector)
{
    static const i18n_key_t titles[] = {
        I18N_CLIMATE_MODE, I18N_CLIMATE_PRESET, I18N_CLIMATE_FAN_MODE,
        I18N_CLIMATE_SWING_MODE, I18N_CLIMATE_HORIZONTAL_SWING
    };
    return selector < sizeof(titles) / sizeof(titles[0])
        ? i18n_get(titles[selector]) : "";
}

static const char *climate_feature_icon(unsigned selector)
{
    static const char *icons[] = {
        MDI_SYMBOL_POWER, MDI_SYMBOL_CREATION, MDI_SYMBOL_WEATHER_WINDY,
        MDI_SYMBOL_SWAP_VERTICAL, MDI_SYMBOL_ARROW_LEFT_RIGHT
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
        {2, "quiet", MDI_SYMBOL_VOLUME_LOW},
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

static void stop_feature_event(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
}

static esp_err_t climate_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id || !instance->config ||
        !climate_feature_supports_config(instance->config)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (climate_feature_is_dropdown(instance->config)) {
        const climate_select_feature_t *feature = select_feature(instance->config->type);
        instance->control = ha_control_select_menu_create(instance->host);
        if (!instance->control) return ESP_ERR_NO_MEM;
        ha_control_select_menu_set_title(instance->control,
                                          climate_feature_title(feature->selector));
        ha_control_select_menu_set_icon(instance->control,
                                         climate_feature_icon(feature->selector));
        ha_control_select_menu_set_disabled(instance->control, true);
        ha_control_select_menu_set_change_cb(instance->control,
                                              climate_menu_option_changed, instance);
    } else {
        instance->control = ha_control_select_create(instance->host);
        if (!instance->control) return ESP_ERR_NO_MEM;
        ha_control_select_set_surface_style(instance->control,
                                             lv_color_hex(0x999999), 71);
        ha_control_select_set_height(instance->control, 56);
        ha_control_select_set_hide_option_label(instance->control, true);
        ha_control_select_set_symbol_font(instance->control, &ha_font_mdi_20);
        ha_control_select_set_text_color(instance->control, lv_color_hex(0xFFFFFF));
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
        uint32_t count = 0;
        const cJSON *mode = NULL;
        cJSON_ArrayForEach(mode, modes) {
            if (!cJSON_IsString(mode) || !mode->valuestring || !mode->valuestring[0] ||
                strlen(mode->valuestring) >= HA_CONTROL_SELECT_MAX_VALUE_LEN) continue;
            if (count == CLIMATE_SELECT_MAX_OPTIONS) break;
            const char *label = entity_state_translate_climate_option(feature->selector,
                                                                       mode->valuestring);
            const char *symbol = climate_option_icon(feature->selector, mode->valuestring);
            if (dropdown) {
                menu_options[count] = (ha_control_select_menu_option_t){
                    .value = mode->valuestring, .label = label, .symbol = symbol
                };
            } else {
                icon_options[count] = (ha_control_select_option_t){
                    .value = mode->valuestring, .label = label, .symbol = symbol
                };
            }
            count++;
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
    if (available) {
        if (feature->value_attribute) {
            ha_entity_store_get_attr_string(store, instance->entity_id,
                                            feature->value_attribute,
                                            selected, sizeof(selected));
        } else {
            lv_strlcpy(selected, snapshot->state, sizeof(selected));
        }
    }
    const lv_color_t color =
        lv_color_hex(hvac_mode_color(snapshot ? snapshot->state : "off"));
    if (dropdown) {
        ha_control_select_menu_set_value(instance->control, selected);
        ha_control_select_menu_set_disabled(instance->control, !available ||
                                            ha_control_select_menu_get_option_count(
                                                instance->control) == 0);
    } else {
        ha_control_select_set_value(instance->control, selected);
        ha_control_select_set_color(instance->control, color);
        ha_control_select_set_disabled(instance->control, !available ||
                                       ha_control_select_get_option_count(instance->control) == 0);
    }
}

static void climate_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (instance && instance->control && instance->config &&
        climate_feature_is_dropdown(instance->config)) {
        ha_control_select_menu_apply_theme(instance->control);
    }
}

const entity_feature_handler_t climate_feature_handler = {
    .supports = climate_feature_supports,
    .supports_config = climate_feature_supports_config,
    .create = climate_feature_create,
    .update = climate_feature_update,
    .apply_theme = climate_feature_apply_theme,
};
