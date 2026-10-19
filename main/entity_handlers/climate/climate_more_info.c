#include "climate_more_info.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "components/climate_dial.h"
#include "components/climate_selector.h"
#include "components/climate_step_buttons.h"
#include "components/climate_target_toggle.h"
#include "esp_log.h"

#define CLIMATE_SELECTOR_COUNT 5
#define CLIMATE_OPTION_MAX 16

typedef struct climate_page climate_page_t;
typedef struct {
    climate_page_t *page;
    size_t index;
} climate_selector_binding_t;

struct climate_page {
    ha_entity_store_t *store;
    lv_obj_t *root;
    lv_obj_t *title;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    bool available;
    bool humidity_mode;
    bool humidity_supported;
    bool select_high;
    bool feedback_active;
    uint32_t feedback_deadline;
    int min_temp;
    int max_temp;
    int temp_step;
    int target_temp;
    int target_temp_low;
    int target_temp_high;
    bool dual_temperature;
    int min_humidity;
    int max_humidity;
    int humidity_step;
    int target_humidity;
    double current_temp;
    double current_humidity;
    bool has_current_temp;
    bool has_current_humidity;
    char hvac_action[HA_ENTITY_STORE_STATE_MAX];
    char hvac_mode[HA_ENTITY_STORE_STATE_MAX];
    lv_obj_t *dial;
    lv_obj_t *steps;
    lv_obj_t *toggle;
    lv_obj_t *selectors[CLIMATE_SELECTOR_COUNT];
    climate_selector_binding_t bindings[CLIMATE_SELECTOR_COUNT];
};

static const char *TAG = "climate_more_info";
static char s_selected_entity[HA_ENTITY_STORE_ENTITY_ID_MAX];
static bool s_humidity_mode;
static climate_page_t *s_active_page;

static const char *s_option_keys[CLIMATE_SELECTOR_COUNT] = {
    "hvac_modes", "preset_modes", "fan_modes", "swing_modes", "swing_horizontal_modes"
};
static const char *s_value_keys[CLIMATE_SELECTOR_COUNT] = {
    NULL, "preset_mode", "fan_mode", "swing_mode", "swing_horizontal_mode"
};

static bool climate_attr_number(climate_page_t *page, const char *key, double *out)
{
    return ha_entity_store_get_attr_number(page->store, page->entity_id, key, out) == ESP_OK &&
           isfinite(*out);
}

static int climate_attr_tenths(climate_page_t *page, const char *key, int fallback)
{
    double value;
    return climate_attr_number(page, key, &value) ? (int)lround(value * 10.0) : fallback;
}

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

static void climate_send_value(climate_page_t *page, const char *service,
                               const char *key, double value)
{
    if (!page || !page->available) return;
    cJSON *data = cJSON_CreateObject();
    if (!data) return;
    if (cJSON_AddNumberToObject(data, key, value)) {
        esp_err_t err = app_ha_call_entity_service_data("climate", service,
                                                        page->entity_id, data);
        if (err != ESP_OK) ESP_LOGW(TAG, "%s failed: %s", service, esp_err_to_name(err));
    }
    cJSON_Delete(data);
}

static void climate_send_string(climate_page_t *page, const char *service,
                                const char *key, const char *value)
{
    if (!page || !page->available || !value) return;
    cJSON *data = cJSON_CreateObject();
    if (!data) return;
    if (cJSON_AddStringToObject(data, key, value)) {
        esp_err_t err = app_ha_call_entity_service_data("climate", service,
                                                        page->entity_id, data);
        if (err != ESP_OK) ESP_LOGW(TAG, "%s failed: %s", service, esp_err_to_name(err));
    }
    cJSON_Delete(data);
}

static void climate_commit_target(climate_page_t *page, int value, bool is_low)
{
    if (!page || !page->available) return;
    if (page->humidity_mode) {
        if (!page->humidity_supported) return;
        int next = LV_CLAMP(page->min_humidity, value, page->max_humidity);
        if (next == page->target_humidity) return;
        page->target_humidity = next;
        page->feedback_active = true;
        page->feedback_deadline = lv_tick_get() + 1500;
        climate_dial_set_value(page->dial, page->target_humidity);
        climate_dial_set_measured(page->dial, page->has_current_humidity
                                              ? (int)lround(page->current_humidity * 10)
                                              : page->target_humidity);
        climate_send_value(page, "set_humidity", "humidity", page->target_humidity / 10.0);
    } else {
        int next = LV_CLAMP(page->min_temp, value, page->max_temp);
        if (page->dual_temperature) {
            if (is_low) {
                next = LV_MIN(next, page->target_temp_high);
                if (next == page->target_temp_low) return;
                page->target_temp_low = next;
                climate_dial_set_value(page->dial, next);
            } else {
                next = LV_MAX(next, page->target_temp_low);
                if (next == page->target_temp_high) return;
                page->target_temp_high = next;
                climate_dial_set_high(page->dial, next);
            }
            cJSON *data = cJSON_CreateObject();
            page->feedback_active = true;
            page->feedback_deadline = lv_tick_get() + 1500;
            if (data) {
                cJSON_AddNumberToObject(data, "target_temp_low", page->target_temp_low / 10.0);
                cJSON_AddNumberToObject(data, "target_temp_high", page->target_temp_high / 10.0);
                esp_err_t err = app_ha_call_entity_service_data("climate", "set_temperature",
                                                                page->entity_id, data);
                if (err != ESP_OK) ESP_LOGW(TAG, "set_temperature failed: %s", esp_err_to_name(err));
                cJSON_Delete(data);
            }
        } else {
            if (next == page->target_temp) return;
            page->target_temp = next;
            page->feedback_active = true;
            page->feedback_deadline = lv_tick_get() + 1500;
            climate_dial_set_value(page->dial, next);
            climate_send_value(page, "set_temperature", "temperature", next / 10.0);
        }
        climate_dial_set_measured(page->dial, page->has_current_temp
                                              ? (int)lround(page->current_temp * 10)
                                              : next);
    }
}

static void climate_dial_changed(int value, bool is_low, void *user_data)
{
    climate_commit_target(user_data, value, is_low);
}

static void climate_selection_changed(bool is_high, void *user_data)
{
    climate_page_t *page = user_data;
    if (page && page->dual_temperature) page->select_high = is_high;
}

static void climate_step_changed(int direction, void *user_data)
{
    climate_page_t *page = user_data;
    if (!page) return;
    int step = page->humidity_mode ? page->humidity_step : page->temp_step;
    bool is_low = !page->dual_temperature || !page->select_high;
    int value = page->humidity_mode ? page->target_humidity
              : page->dual_temperature ? (is_low ? page->target_temp_low
                                                 : page->target_temp_high)
                                       : page->target_temp;
    climate_commit_target(page, value + direction * step, is_low);
}

static void climate_refresh_dial(climate_page_t *page)
{
    char current_temp[48] = "";
    char current_humidity[48] = "";
    if (page->has_current_temp)
        snprintf(current_temp, sizeof(current_temp), "Current %.1f°C", page->current_temp);
    if (page->has_current_humidity)
        snprintf(current_humidity, sizeof(current_humidity), "Humidity %.1f%%", page->current_humidity);
    if (page->humidity_mode) {
        climate_dial_set_dual(page->dial, false);
        climate_dial_set_range(page->dial, page->min_humidity,
                               page->max_humidity, page->humidity_step);
        climate_dial_set_unit(page->dial, "%");
        climate_dial_set_value(page->dial, page->target_humidity);
        climate_dial_set_measured(page->dial, page->has_current_humidity
                                              ? (int)lround(page->current_humidity * 10)
                                              : page->target_humidity);
    } else {
        climate_dial_set_dual(page->dial, page->dual_temperature);
        climate_dial_set_range(page->dial, page->min_temp, page->max_temp, page->temp_step);
        climate_dial_set_unit(page->dial, "°C");
        climate_dial_set_value(page->dial, page->dual_temperature
                                              ? page->target_temp_low : page->target_temp);
        if (page->dual_temperature)
            climate_dial_set_high(page->dial, page->target_temp_high);
        climate_dial_set_selected_high(page->dial, page->dual_temperature && page->select_high);
        climate_dial_set_measured(page->dial, page->has_current_temp
                                              ? (int)lround(page->current_temp * 10)
                                              : page->target_temp);
    }
    climate_dial_set_currents(page->dial, current_temp, current_humidity);
    climate_dial_set_action(page->dial, page->hvac_action);
    climate_dial_set_theme(page->dial, page->hvac_mode, page->humidity_mode);
    bool controls_enabled = page->available;
    climate_dial_set_disabled(page->dial, !controls_enabled ||
                             (page->humidity_mode && !page->humidity_supported));
    climate_step_buttons_set_disabled(page->steps, !controls_enabled ||
                                      (page->humidity_mode && !page->humidity_supported));
    climate_target_toggle_set_selected(page->toggle, page->humidity_mode);
}

static void climate_refresh_toggle(climate_page_t *page)
{
    climate_target_toggle_set_humidity_available(page->toggle, page->humidity_supported);
    if (page->humidity_supported)
        lv_obj_remove_flag(page->toggle, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(page->toggle, LV_OBJ_FLAG_HIDDEN);
}

static void climate_target_changed(bool humidity, void *user_data)
{
    climate_page_t *page = user_data;
    if (!page) return;
    page->humidity_mode = humidity;
    s_humidity_mode = humidity;
    climate_refresh_dial(page);
}

static void climate_selector_changed(size_t index, const char *value, void *user_data)
{
    (void)index;
    climate_selector_binding_t *binding = user_data;
    if (!binding || !binding->page) return;
    static const char *services[] = {
        "set_hvac_mode", "set_preset_mode", "set_fan_mode",
        "set_swing_mode", "set_swing_horizontal_mode"
    };
    static const char *keys[] = {
        "hvac_mode", "preset_mode", "fan_mode", "swing_mode", "swing_horizontal_mode"
    };
    climate_send_string(binding->page, services[binding->index], keys[binding->index], value);
    if (binding->index == 0 && value) {
        snprintf(binding->page->hvac_mode, sizeof(binding->page->hvac_mode), "%s", value);
        climate_refresh_dial(binding->page);
    }
}

static void climate_close_clicked(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) context->close(context->close_user_data);
}

static void climate_page_deleted(lv_event_t *event)
{
    climate_page_t *page = lv_obj_get_user_data(lv_event_get_target_obj(event));
    if (s_active_page == page) s_active_page = NULL;
    lv_free(page);
}

typedef struct {
    size_t selector;
    const char *value;
    const char *icon;
} climate_option_icon_t;

static const climate_option_icon_t s_option_icons[] = {
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

static const char *climate_option_icon(size_t selector, const char *value)
{
    char mode[48];
    while (*value == ' ') value++;
    size_t length = strlen(value);
    while (length && value[length - 1] == ' ') length--;
    length = LV_MIN(length, sizeof(mode) - 1);
    for (size_t i = 0; i < length; ++i)
        mode[i] = (char)tolower((unsigned char)value[i]);
    mode[length] = '\0';
    for (size_t i = 0; i < sizeof(s_option_icons) / sizeof(s_option_icons[0]); ++i) {
        if (s_option_icons[i].selector == selector &&
            strcmp(s_option_icons[i].value, mode) == 0)
            return s_option_icons[i].icon;
    }
    return MDI_SYMBOL_CIRCLE_MEDIUM;
}

static bool climate_set_selector_options(climate_page_t *page, size_t index,
                                         const char *attribute, const char *selected)
{
    cJSON *array = NULL;
    if (ha_entity_store_dup_attr_json(page->store, page->entity_id, attribute, &array) != ESP_OK ||
        !cJSON_IsArray(array)) {
        climate_selector_set_disabled(page->selectors[index], true);
        cJSON_Delete(array);
        return false;
    }
    const char *options[CLIMATE_OPTION_MAX];
    const char *icons[CLIMATE_OPTION_MAX];
    size_t count = 0;
    size_t selected_index = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || !item->valuestring || !item->valuestring[0] ||
            count >= CLIMATE_OPTION_MAX) continue;
        options[count] = item->valuestring;
        icons[count] = climate_option_icon(index, item->valuestring);
        if (selected && strcmp(selected, item->valuestring) == 0) selected_index = count;
        count++;
    }
    bool supported = count &&
        climate_selector_set_options(page->selectors[index], options, icons, count);
    if (supported) {
        climate_selector_set_selected(page->selectors[index], selected_index);
        climate_selector_set_disabled(page->selectors[index], !page->available);
    } else {
        climate_selector_set_disabled(page->selectors[index], true);
    }
    cJSON_Delete(array);
    return supported;
}

static void climate_layout_selectors(climate_page_t *page,
                                     const size_t *visible, size_t count)
{
    for (size_t slot = 0; slot < count; ++slot) {
        int x, y, width;
        if (count <= 2 || (count == 3 && slot == 0)) {
            x = 23;
            y = 356 + (int)slot * 60;
            width = 274;
        } else if (count == 5) {
            x = slot < 3 ? 7 + (int)slot * 104 : 58 + (int)(slot - 3) * 106;
            y = slot < 3 ? 356 : 416;
            width = slot < 3 ? 98 : 100;
        } else {
            size_t column = count == 3 ? slot - 1 : slot % 2;
            x = column ? 164 : 23;
            y = count == 3 || slot >= 2 ? 416 : 356;
            width = 133;
        }
        lv_obj_t *selector = page->selectors[visible[slot]];
        lv_obj_set_pos(selector, x, y);
        lv_obj_set_size(selector, width, 52);
        climate_selector_set_content_layout(selector, width >= 200);
    }
}

static void climate_refresh_selectors(climate_page_t *page)
{
    size_t visible[CLIMATE_SELECTOR_COUNT];
    size_t visible_count = 0;
    for (size_t i = 0; i < CLIMATE_SELECTOR_COUNT; ++i) {
        if (!page->selectors[i]) continue;
        char current[48] = "";
        const char *selected = page->hvac_mode;
        if (s_value_keys[i]) {
            ha_entity_store_get_attr_string(page->store, page->entity_id,
                                            s_value_keys[i], current, sizeof(current));
            selected = current;
        }
        bool supported = climate_set_selector_options(page, i, s_option_keys[i], selected);
        if (supported) {
            lv_obj_remove_flag(page->selectors[i], LV_OBJ_FLAG_HIDDEN);
            visible[visible_count++] = i;
        } else {
            lv_obj_add_flag(page->selectors[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    climate_layout_selectors(page, visible, visible_count);
}

static void climate_read_attributes(climate_page_t *page, const ha_entity_snapshot_t *snapshot)
{
    snprintf(page->hvac_mode, sizeof(page->hvac_mode), "%s", snapshot->state);
    page->available = !entity_handler_snapshot_is_unavailable(snapshot);
    page->has_current_temp = climate_attr_number(page, "current_temperature", &page->current_temp);
    page->has_current_humidity = climate_attr_number(page, "current_humidity", &page->current_humidity);
    page->min_temp = climate_attr_tenths(page, "min_temp", 160);
    page->max_temp = climate_attr_tenths(page, "max_temp", 300);
    page->temp_step = LV_MAX(1, climate_attr_tenths(page, "target_temp_step", 5));
    if (page->max_temp <= page->min_temp) page->max_temp = page->min_temp + page->temp_step;
    page->temp_step = LV_MIN(page->temp_step, page->max_temp - page->min_temp);
    double target, low, high;
    bool has_target = climate_attr_number(page, "temperature", &target);
    bool has_low = climate_attr_number(page, "target_temp_low", &low);
    bool has_high = climate_attr_number(page, "target_temp_high", &high);
    page->dual_temperature = (!has_target || target <= 0) &&
                             has_low && low > 0 && has_high && high > 0;
    if (page->dual_temperature) {
        page->target_temp_low = LV_CLAMP(page->min_temp, (int)lround(low * 10), page->max_temp);
        page->target_temp_high = LV_CLAMP(page->min_temp, (int)lround(high * 10), page->max_temp);
        if (page->target_temp_low > page->target_temp_high)
            page->target_temp_low = page->target_temp_high;
        page->target_temp = page->target_temp_low;
    } else {
        page->target_temp = has_target && target > 0
            ? LV_CLAMP(page->min_temp, (int)lround(target * 10), page->max_temp)
            : (int)lround((page->min_temp + page->max_temp) / 2.0);
    }
    page->min_humidity = climate_attr_tenths(page, "min_humidity", 0);
    page->max_humidity = climate_attr_tenths(page, "max_humidity", 1000);
    page->humidity_step = LV_MAX(1, climate_attr_tenths(page, "target_humidity_step", 10));
    if (page->max_humidity <= page->min_humidity)
        page->max_humidity = page->min_humidity + page->humidity_step;
    double humidity;
    page->humidity_supported = climate_attr_number(page, "humidity", &humidity);
    page->target_humidity = page->humidity_supported ? (int)lround(humidity * 10) : 500;
    char raw_action[HA_ENTITY_STORE_STATE_MAX] = "";
    ha_entity_store_get_attr_string(page->store, page->entity_id, "hvac_action",
                                    raw_action, sizeof(raw_action));
    const char *display = NULL;
    if (strcmp(raw_action, "heating") == 0) display = "Heating";
    else if (strcmp(raw_action, "cooling") == 0) display = "Cooling";
    else if (strcmp(raw_action, "drying") == 0) display = "Drying";
    else if (strcmp(raw_action, "fan") == 0) display = "Fan";
    else if (strcmp(raw_action, "off") == 0) display = "Off";
    else if (strcmp(page->hvac_mode, "heat") == 0) display = "Heat";
    else if (strcmp(page->hvac_mode, "cool") == 0) display = "Cool";
    else if (strcmp(page->hvac_mode, "heat_cool") == 0) display = "Heat/Cool";
    else if (strcmp(page->hvac_mode, "auto") == 0) display = "Auto";
    else if (strcmp(page->hvac_mode, "dry") == 0) display = "Dry";
    else if (strcmp(page->hvac_mode, "fan_only") == 0) display = "Fan Only";
    else if (strcmp(page->hvac_mode, "off") == 0) display = "Off";
    else display = raw_action[0] ? raw_action : "Idle";
    snprintf(page->hvac_action, sizeof(page->hvac_action), "%s", display);
}

esp_err_t climate_more_info_update(const entity_more_info_context_t *context)
{
    climate_page_t *page = s_active_page;
    if (!context || !page || !page->root || !page->dial ||
        lv_obj_get_parent(page->root) != context->screen ||
        strcmp(page->entity_id, context->snapshot.entity_id) != 0)
        return ESP_ERR_INVALID_STATE;

    int old_temp = page->target_temp;
    int old_low = page->target_temp_low;
    int old_high = page->target_temp_high;
    int old_humidity = page->target_humidity;
    bool old_dual = page->dual_temperature;
    bool dragging = climate_dial_is_dragging(page->dial);

    page->store = context->store;
    climate_read_attributes(page, &context->snapshot);
    bool target_confirmed = old_dual == page->dual_temperature &&
        (page->humidity_mode ? old_humidity == page->target_humidity
         : old_dual ? old_low == page->target_temp_low &&
                      old_high == page->target_temp_high
                    : old_temp == page->target_temp);
    if (page->feedback_active &&
        (target_confirmed ||
         (int32_t)(page->feedback_deadline - lv_tick_get()) <= 0))
        page->feedback_active = false;
    if (dragging || page->feedback_active) {
        page->target_temp = old_temp;
        page->target_temp_low = old_low;
        page->target_temp_high = old_high;
        page->target_humidity = old_humidity;
        page->dual_temperature = old_dual;
    }

    if (page->title)
        lv_label_set_text(page->title, context->snapshot.friendly_name[0]
                                      ? context->snapshot.friendly_name : page->entity_id);
    climate_refresh_toggle(page);
    if (!dragging) {
        if (!page->humidity_supported) page->humidity_mode = s_humidity_mode = false;
        climate_refresh_dial(page);
    }
    climate_refresh_selectors(page);
    return ESP_OK;
}

esp_err_t climate_more_info_render(const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay || !context->store || !context->close)
        return ESP_ERR_INVALID_ARG;
    climate_page_t *page = lv_calloc(1, sizeof(*page));
    if (!page) return ESP_ERR_NO_MEM;
    page->store = context->store;
    snprintf(page->entity_id, sizeof(page->entity_id), "%s", context->snapshot.entity_id);
    if (strcmp(s_selected_entity, page->entity_id) != 0) {
        snprintf(s_selected_entity, sizeof(s_selected_entity), "%s", page->entity_id);
        s_humidity_mode = false;
    }
    page->humidity_mode = s_humidity_mode;
    climate_read_attributes(page, &context->snapshot);
    if (!page->humidity_supported) page->humidity_mode = s_humidity_mode = false;

    lv_obj_t *screen = context->screen;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x010203), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));

    lv_obj_t *root = lv_obj_create(screen);
    if (!root) { lv_free(page); return ESP_ERR_NO_MEM; }
    page->root = root;
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(root, page);
    lv_obj_add_event_cb(root, climate_page_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *header = lv_obj_create(root);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, LV_PCT(100), 76);
    lv_obj_t *close = lv_image_create(header);
    lv_image_set_src(close, &close_40);
    lv_obj_set_pos(close, 20, 18);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close, climate_close_clicked, LV_EVENT_CLICKED, (void *)context);
    lv_obj_t *title = lv_label_create(header);
    page->title = title;
    lv_obj_set_pos(title, 72, 22);
    lv_obj_set_width(title, 228);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title, context->snapshot.friendly_name[0] ?
                            context->snapshot.friendly_name : page->entity_id);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *close_hitbox = lv_obj_create(context->overlay);
    lv_obj_remove_style_all(close_hitbox);
    lv_obj_remove_flag(close_hitbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(close_hitbox, 40, 40);
    lv_obj_align(close_hitbox, LV_ALIGN_TOP_LEFT, 20, 18);
    lv_obj_add_event_cb(close_hitbox, climate_close_clicked, LV_EVENT_CLICKED, (void *)context);

    page->dial = climate_dial_create(root);
    page->steps = climate_step_buttons_create(page->dial);
    page->toggle = climate_target_toggle_create(root);
    if (!page->dial || !page->steps || !page->toggle) return ESP_ERR_NO_MEM;
    lv_obj_align(page->dial, LV_ALIGN_TOP_MID, 0, 74);
    lv_obj_add_flag(page->steps, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(page->steps, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_align(page->toggle, LV_ALIGN_TOP_MID, 0, 300);
    climate_dial_set_change_cb(page->dial, climate_dial_changed, page);
    climate_dial_set_selection_cb(page->dial, climate_selection_changed, page);
    climate_step_buttons_set_callback(page->steps, climate_step_changed, page);
    climate_target_toggle_set_callback(page->toggle, climate_target_changed, page);
    climate_refresh_toggle(page);
    climate_refresh_dial(page);

    static const char *titles[] = {
        "Mode", "Preset", "Fan mode", "Swing mode", "Horizontal swing"
    };
    static const char *icons[] = {
        MDI_SYMBOL_POWER, MDI_SYMBOL_CREATION, MDI_SYMBOL_WEATHER_WINDY,
        MDI_SYMBOL_SWAP_VERTICAL, MDI_SYMBOL_ARROW_LEFT_RIGHT
    };
    for (size_t i = 0; i < CLIMATE_SELECTOR_COUNT; ++i) {
        lv_obj_t *selector = climate_selector_create(root);
        if (!selector) return ESP_ERR_NO_MEM;
        page->selectors[i] = selector;
        lv_obj_add_flag(selector, LV_OBJ_FLAG_HIDDEN);
        climate_selector_set_title(selector, titles[i]);
        climate_selector_set_icon(selector, icons[i]);
        page->bindings[i] = (climate_selector_binding_t){ .page = page, .index = i };
        climate_selector_set_change_cb(selector, climate_selector_changed, &page->bindings[i]);
    }
    climate_refresh_selectors(page);
    s_active_page = page;
    return ESP_OK;
}
