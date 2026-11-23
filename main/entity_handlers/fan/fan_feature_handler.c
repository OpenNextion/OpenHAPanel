#include "fan_feature_handler.h"

#include <ctype.h>
#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "common/ha_control_select/ha_control_select.h"
#include "common/ha_control_select_menu/ha_control_select_menu.h"
#include "common/ha_control_slider/ha_control_slider.h"
#include "common/ha_control_toggle/ha_control_toggle.h"
#include "esp_log.h"
#include "i18n.h"
#include "screen_theme.h"

static const char *TAG = "fan_feature_handler";

#define FAN_SELECT_MAX_OPTIONS 16

#define FAN_CONTROL_ON_COLOR        0x08AECA
#define FAN_CONTROL_OFF_COLOR       0x999999
#define FAN_CONTROL_INACTIVE_FG     0x666666
#define FAN_LIGHT_CONTROL_ON_COLOR  0x00bcd4
#define FAN_LIGHT_CONTROL_OFF_COLOR 0x9e9e9e
#define FAN_LIGHT_INACTIVE_FG       0x141414
#define FAN_LIGHT_SPEED_TRACK_ON_BG 0xccf2f6
#define FAN_LIGHT_SPEED_TRACK_OFF_BG 0xE3E3E3
#define FAN_HA_SPEED_COUNT_MAX       11
#define FAN_HA_SPEED_SWITCH_MAX       1
#define FAN_HA_SPEED_STEPS_MAX        5

#define FAN_SELECT_TRACK_BG_LIGHT   0xD6D6D6
#define FAN_SELECT_TRACK_BG_DARK    0x242424
#define FAN_SELECT_TRACK_OPA        255
#define FAN_UNAVAILABLE_SURFACE_DARK 0x242424
#define FAN_UNAVAILABLE_ITEM_DARK    0x464646
#define FAN_UNAVAILABLE_ICON_DARK    0x8D8D8D

#define FAN_SPEED_OPTION_RADIUS     8
#define FAN_SPEED_OPTION_GAP        4

typedef enum {
    FAN_SPEED_STYLE_SLIDER,
    FAN_SPEED_STYLE_SWITCH,
    FAN_SPEED_STYLE_STEPS,
} fan_speed_style_t;

typedef struct {
    screen_feature_type_t type;
    fan_speed_style_t speed_style;
    char entity_id[128];
    lv_obj_t *track;
    lv_obj_t *touch_slider;
    lv_obj_t *speed_switch;
    lv_obj_t *options[FAN_HA_SPEED_COUNT_MAX + 1];
    lv_obj_t *option_icons[FAN_HA_SPEED_COUNT_MAX + 1];
    int speed_count;
    int steps_total;
    int percentage;
    bool on;
    bool available;
} fan_feature_data_t;

static void fan_update_speed_slider_visual(entity_feature_instance_t *instance,
                                           fan_feature_data_t *data);

static bool fan_light_theme(void)
{
    return screen_theme_get() == SCREEN_THEME_LIGHT;
}

static uint32_t fan_select_surface_color(void)
{
    return fan_light_theme() ? 0xE3E3E3 : FAN_SELECT_TRACK_BG_DARK;
}

static uint32_t fan_toggle_on_knob(void)
{
    return fan_light_theme() ? FAN_LIGHT_CONTROL_ON_COLOR : FAN_CONTROL_ON_COLOR;
}

static uint32_t fan_toggle_off_knob(void)
{
    return fan_light_theme() ? FAN_LIGHT_CONTROL_OFF_COLOR : FAN_CONTROL_OFF_COLOR;
}

static uint32_t fan_select_selected_color(bool entity_on, bool available)
{
    if (!available) return 0xBDBDBD;
    return entity_on ? fan_toggle_on_knob() : fan_toggle_off_knob();
}

static uint32_t fan_control_off_color(void)
{
    return fan_light_theme() ? FAN_LIGHT_CONTROL_OFF_COLOR : FAN_CONTROL_OFF_COLOR;
}

static uint32_t fan_control_on_color(void)
{
    return fan_light_theme() ? FAN_LIGHT_CONTROL_ON_COLOR : FAN_CONTROL_ON_COLOR;
}

static uint32_t fan_inactive_fg(void)
{
    return fan_light_theme() ? FAN_LIGHT_INACTIVE_FG : FAN_CONTROL_INACTIVE_FG;
}

static uint32_t fan_speed_inactive_fg(bool available)
{
    if (!available) {
        return fan_light_theme() ? 0xBDBDBD : FAN_UNAVAILABLE_ITEM_DARK;
    }
    return fan_inactive_fg();
}

static void fan_apply_unavailable_icons_style(lv_obj_t *control,
                                              bool available)
{
    if (!control) return;

    if (available) {
        ha_control_select_set_surface_style(
            control,
            lv_color_hex(fan_select_surface_color()),
            LV_OPA_100);
        ha_control_select_set_text_color(
            control,
            lv_color_hex(fan_light_theme() ? 0x141414 : 0xE1E1E1));
        ha_control_select_set_disabled_opa(control, LV_OPA_40);
        lv_obj_set_style_opa(control, LV_OPA_COVER, LV_PART_MAIN);
        return;
    }

    const bool light = fan_light_theme();
    const lv_color_t surface = lv_color_hex(fan_select_surface_color());
    const lv_color_t unavailable = lv_color_hex(
        light ? 0xBDBDBD : FAN_UNAVAILABLE_ITEM_DARK);

    /* Buttonmatrix draw items do not consistently inherit DISABLED in their
     * draw-part state.  Style the actual CHECKED selector and keep the root
     * fully opaque so the requested RGB values reach the display unchanged. */
    ha_control_select_set_disabled_opa(control, LV_OPA_COVER);
    ha_control_select_set_surface_style(control, surface, LV_OPA_100);
    ha_control_select_set_text_color(control, unavailable);
    ha_control_select_set_color(control, unavailable);
    lv_obj_set_style_text_color(control, lv_color_white(),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(control, lv_color_white(),
                                LV_PART_ITEMS | LV_STATE_CHECKED |
                                    LV_STATE_PRESSED);
    lv_obj_set_style_opa(control, LV_OPA_COVER, LV_PART_MAIN);
}

static void fan_apply_dropdown_availability_style(lv_obj_t *control,
                                                  bool available)
{
    if (!control) return;

    for (int light = 0; light <= 1; light++) {
        ha_control_select_menu_colors_t colors;
        if (!ha_control_select_menu_get_colors(control, light != 0, &colors)) {
            continue;
        }
        colors.button_color = light ? 0xE3E3E3 : FAN_SELECT_TRACK_BG_DARK;
        colors.button_opa = light || available ? LV_OPA_COVER : LV_OPA_COVER;
        colors.text_color = available
                                ? (light ? 0x333333 : 0xFFFFFF)
                                : (light ? 0xBDBDBD
                                         : FAN_UNAVAILABLE_ITEM_DARK);
        ha_control_select_menu_set_colors(control, light != 0, &colors);
    }
}

static fan_feature_data_t *get_data(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static bool fan_feature_supports_config(const screen_feature_config_t *config)
{
    if (!config) return false;
    switch (config->type) {
    case SCREEN_FEATURE_TOGGLE:
    case SCREEN_FEATURE_FAN_SPEED:
    case SCREEN_FEATURE_FAN_PRESET_MODES:
    case SCREEN_FEATURE_FAN_DIRECTION:
    case SCREEN_FEATURE_FAN_OSCILLATION:
        return true;
    default:
        return false;
    }
}

static bool fan_feature_is_dropdown(const screen_feature_config_t *config)
{
    if (!config) return false;
    const cJSON *style = config->data
        ? cJSON_GetObjectItemCaseSensitive(config->data, "style") : NULL;
    if (cJSON_IsString(style) && style->valuestring && style->valuestring[0]) {
        return strcmp(style->valuestring, "dropdown") == 0;
    }
    /* Fallback default when the frontend did not pass a style. */
    return config->type == SCREEN_FEATURE_FAN_PRESET_MODES;
}

static bool fan_feature_supports(ha_entity_store_t *store,
                                 const char *entity_id,
                                 screen_feature_type_t type)
{
    if (!store || !entity_id) return true;

    double num = 0;
    uint32_t supported_features = 0;
    if (ha_entity_store_get_attr_number(store, entity_id,
                                        "supported_features", &num) == ESP_OK) {
        supported_features = (uint32_t)num;
    }

    switch (type) {
    case SCREEN_FEATURE_TOGGLE:
        return true;
    case SCREEN_FEATURE_FAN_SPEED:
        return (supported_features & (1U << 0)) != 0;
    case SCREEN_FEATURE_FAN_PRESET_MODES:
        return (supported_features & (1U << 3)) != 0;
    case SCREEN_FEATURE_FAN_DIRECTION:
        return (supported_features & (1U << 2)) != 0;
    case SCREEN_FEATURE_FAN_OSCILLATION:
        return (supported_features & (1U << 1)) != 0;
    default:
        return false;
    }
}

static void fan_call_service(const char *entity_id,
                             const char *service,
                             cJSON *data)
{
    if (!entity_id || !entity_id[0]) return;
    esp_err_t err = app_ha_call_entity_service_data("fan", service,
                                                    entity_id, data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "fan.%s for %s failed: %s",
                 service, entity_id, esp_err_to_name(err));
    }
}

static void toggle_changed(bool on, void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id) return;
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store ||
        ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) return;

    fan_call_service(instance->entity_id,
                     on ? "turn_on" : "turn_off", NULL);
}

static void fan_option_changed(lv_event_t *event)
{
    entity_feature_instance_t *instance = lv_event_get_user_data(event);
    if (!instance || !instance->control) return;
    const char *value = ha_control_select_get_value(instance->control);
    if (!value || !value[0]) return;
    ha_entity_store_t *store = app_get_ha_entity_store();
    if (!store || !instance->entity_id) return;
    ha_entity_snapshot_t snapshot = {0};
    if (ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) return;

    screen_feature_type_t type = instance->config ? instance->config->type
                                                    : SCREEN_FEATURE_INVALID;
    if (type == SCREEN_FEATURE_FAN_OSCILLATION) {
        cJSON *data = cJSON_CreateObject();
        if (!data) return;
        bool oscillating = strcmp(value, "on") == 0;
        cJSON_AddBoolToObject(data, "oscillating", oscillating);
        fan_call_service(instance->entity_id, "oscillate", data);
        cJSON_Delete(data);
    } else if (type == SCREEN_FEATURE_FAN_DIRECTION) {
        cJSON *data = cJSON_CreateObject();
        if (!data) return;
        cJSON_AddStringToObject(data, "direction", value);
        fan_call_service(instance->entity_id, "set_direction", data);
        cJSON_Delete(data);
    } else if (type == SCREEN_FEATURE_FAN_PRESET_MODES) {
        cJSON *data = cJSON_CreateObject();
        if (!data) return;
        cJSON_AddStringToObject(data, "preset_mode", value);
        fan_call_service(instance->entity_id, "set_preset_mode", data);
        cJSON_Delete(data);
    } else if (type == SCREEN_FEATURE_FAN_SPEED) {
        bool on = strcmp(value, "on") == 0;
        if (!on) {
            fan_call_service(instance->entity_id, "turn_off", NULL);
        } else {
            cJSON *data = cJSON_CreateObject();
            if (data) {
                cJSON_AddNumberToObject(data, "percentage", 100);
                fan_call_service(instance->entity_id, "set_percentage", data);
                cJSON_Delete(data);
            }
        }
    }
}

static void fan_menu_option_changed(const char *value, void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id || !instance->config) return;
    if (!value || !value[0]) return;
    ha_entity_store_t *store = app_get_ha_entity_store();
    if (!store) return;
    ha_entity_snapshot_t snapshot = {0};
    if (ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) return;

    screen_feature_type_t type = instance->config->type;
    if (type == SCREEN_FEATURE_FAN_PRESET_MODES) {
        cJSON *data = cJSON_CreateObject();
        if (!data) return;
        cJSON_AddStringToObject(data, "preset_mode", value);
        fan_call_service(instance->entity_id, "set_preset_mode", data);
        cJSON_Delete(data);
    }
}

static void stop_feature_event(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
}

static const char *fan_speed_icon(int index, int total)
{
    int opt_count = total;
    if (opt_count == 2) {
        return index == 0 ? MDI_SYMBOL_FAN_OFF : MDI_SYMBOL_FAN;
    } else if (opt_count == 3) {
        static const char *icons[] = {MDI_SYMBOL_FAN_OFF,
                                      MDI_SYMBOL_FAN_SPEED_1,
                                      MDI_SYMBOL_FAN_SPEED_2};
        return index < 3 ? icons[index] : MDI_SYMBOL_FAN_OFF;
    } else if (opt_count == 4) {
        static const char *icons[] = {MDI_SYMBOL_FAN_OFF,
                                      MDI_SYMBOL_FAN_SPEED_1,
                                      MDI_SYMBOL_FAN_SPEED_2,
                                      MDI_SYMBOL_FAN_SPEED_3};
        return index < 4 ? icons[index] : MDI_SYMBOL_FAN_OFF;
    } else if (opt_count == 5) {
        static const char *items[] = {MDI_SYMBOL_FAN_OFF,
                                      "1", "2", "3", "4"};
        return index < 5 ? items[index] : MDI_SYMBOL_FAN_OFF;
    } else if (opt_count == 6) {
        static const char *items[] = {MDI_SYMBOL_FAN_OFF,
                                      "1", "2", "3", "4", "5"};
        return index < 6 ? items[index] : MDI_SYMBOL_FAN_OFF;
    } else {
        static const char *icons[] = {MDI_SYMBOL_FAN_OFF,
                                      MDI_SYMBOL_FAN_SPEED_1,
                                      MDI_SYMBOL_FAN_SPEED_2,
                                      MDI_SYMBOL_FAN_SPEED_3,
                                      MDI_SYMBOL_FAN,
                                      MDI_SYMBOL_FAN};
        return index < 6 ? icons[index] : MDI_SYMBOL_FAN_OFF;
    }
}

static bool fan_speed_option_is_numeric(int index, int total)
{
    if (total == 5) return index >= 1 && index < 5;
    if (total == 6) return index >= 1 && index < 6;
    return false;
}

static int fan_speed_selected_index(const fan_feature_data_t *data, int total)
{
    if (!data->on || data->percentage <= 0) return 0;
    const int counted = total - 1;
    int selected = (data->percentage * counted + 99) / 100;
    if (selected < 1) selected = 1;
    if (selected > counted) selected = counted;
    return selected;
}

static void fan_speed_option_click(lv_event_t *event)
{
    fan_feature_data_t *data = lv_event_get_user_data(event);
    lv_obj_t *option = lv_event_get_current_target_obj(event);
    if (!data || !option || !data->available) return;
    int index = (int)(uintptr_t)lv_obj_get_user_data(option);
    int total = data->speed_count + 1;
    if (index < 0 || index >= total) return;
    const int counted = total - 1;
    if (total == 2) {
        data->percentage = (index == 0) ? 0 : 100;
    } else {
        data->percentage = (index == 0) ? 0 : (index * 100) / counted;
    }
    data->on = data->percentage > 0;

    cJSON *svc = NULL;
    if (!data->on) {
        fan_call_service(data->entity_id, "turn_off", NULL);
    } else {
        svc = cJSON_CreateObject();
        if (svc) {
            cJSON_AddNumberToObject(svc, "percentage", data->percentage);
            fan_call_service(data->entity_id, "set_percentage", svc);
            cJSON_Delete(svc);
        }
    }
}

static void fan_speed_slider_changed(uint8_t percent,
                                     ha_control_slider_phase_t phase,
                                     void *user_data)
{
    if (phase != HA_CONTROL_SLIDER_COMMIT) {
        return;
    }
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id) return;
    fan_feature_data_t *data = get_data(instance->host);
    if (!data || !data->available) return;

    if (percent == 0) {
        fan_call_service(data->entity_id, "turn_off", NULL);
    } else {
        cJSON *svc = cJSON_CreateObject();
        if (svc) {
            cJSON_AddNumberToObject(svc, "percentage", percent);
            fan_call_service(data->entity_id, "set_percentage", svc);
            cJSON_Delete(svc);
        }
    }
}

static void fan_update_speed_slider_visual(entity_feature_instance_t *instance,
                                           fan_feature_data_t *data)
{
    lv_obj_t *slider = instance ? instance->control : NULL;
    if (!slider || !data) return;

    const int percent = LV_CLAMP(0, data->percentage, 100);
    ha_control_slider_set_value(slider, (uint8_t)percent, LV_ANIM_OFF);
    ha_control_slider_set_active_color_raw(
        slider, lv_color_hex(data->on ? fan_control_on_color()
                                      : fan_control_off_color()));
    ha_control_slider_set_power(slider, data->on);
    ha_control_slider_set_disabled(slider, !data->available);
}

static void fan_update_speed_switch(entity_feature_instance_t *instance,
                                    fan_feature_data_t *data)
{
    lv_obj_t *control = instance ? instance->control : NULL;
    if (!control || !data) return;

    const char *options[] = {i18n_get(I18N_STATE_OFF), i18n_get(I18N_STATE_ON)};
    const char *icons[] = {MDI_SYMBOL_FAN_OFF, MDI_SYMBOL_FAN};

    ha_control_select_option_t icon_opts[2] = {0};
    icon_opts[0] = (ha_control_select_option_t){
        .value = "off", .label = options[0], .symbol = icons[0]};
    icon_opts[1] = (ha_control_select_option_t){
        .value = "on", .label = options[1], .symbol = icons[1]};
    ha_control_select_set_options_array(control, icon_opts, 2);

    ha_control_select_set_value(control, data->on ? "on" : "off");
    ha_control_select_set_color(control,
        lv_color_hex(fan_select_selected_color(data->on, data->available)));
    ha_control_select_set_disabled(control, !data->available);
    fan_apply_unavailable_icons_style(control, data->available);

    /* Restore the hand-drawn switch's track tint: the select surface would
     * otherwise stay a neutral color regardless of on/off state. */
    if (data->available) {
        if (data->on) {
            ha_control_select_set_surface_style(control,
                lv_color_hex(fan_control_on_color()), LV_OPA_30);
        } else if (fan_light_theme()) {
            ha_control_select_set_surface_style(control,
                lv_color_hex(FAN_LIGHT_SPEED_TRACK_OFF_BG), LV_OPA_COVER);
        } else {
            ha_control_select_set_surface_style(control,
                lv_color_hex(fan_control_off_color()), LV_OPA_20);
        }
    }
}

static void fan_update_speed_steps_visual(fan_feature_data_t *data)
{
    int total = data->speed_count + 1;
    int selected = fan_speed_selected_index(data, total);
    bool is_off = (selected == 0);
    if (data->track) {
        uint32_t track_color;
        lv_opa_t track_opa;
        if (is_off && fan_light_theme()) {
            track_color = FAN_LIGHT_SPEED_TRACK_OFF_BG;
            track_opa = LV_OPA_COVER;
        } else {
            track_color = is_off ? fan_control_off_color()
                                 : fan_control_on_color();
            track_opa = is_off ? LV_OPA_20 : LV_OPA_30;
        }
        lv_obj_set_style_bg_color(data->track, lv_color_hex(track_color), 0);
        lv_obj_set_style_bg_opa(data->track, track_opa, 0);
        /* Keep unavailable option icons at their exact requested RGB values.
         * Parent opacity would also dim every child icon. */
        lv_obj_set_style_opa(data->track, LV_OPA_COVER, 0);
    }
    for (int i = 0; i < total; i++) {
        if (!data->options[i]) continue;
        bool active = i == selected;
        lv_obj_set_style_bg_color(data->options[i],
            lv_color_hex(active
                ? (is_off ? fan_control_off_color() : fan_control_on_color())
                : 0x000000),
            0);
        lv_obj_set_style_bg_opa(data->options[i],
            active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (data->option_icons[i]) {
            lv_obj_set_style_text_color(data->option_icons[i],
                lv_color_hex(active ? 0xFFFFFF
                                    : fan_speed_inactive_fg(data->available)),
                0);
        }
    }
}

static void fan_speed_delete_cb(lv_event_t *event)
{
    fan_feature_data_t *data = lv_event_get_user_data(event);
    if (data) lv_free(data);
}

static esp_err_t fan_create_toggle(entity_feature_instance_t *instance)
{
    instance->control = ha_control_toggle_create(instance->host);
    if (!instance->control) return ESP_ERR_NO_MEM;

    lv_obj_remove_flag(instance->control, LV_OBJ_FLAG_SCROLLABLE |
                                          LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(instance->control, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(instance->control, stop_feature_event,
                        LV_EVENT_ALL, NULL);

    ha_control_toggle_set_light_colors(
        instance->control,
        lv_color_hex(FAN_LIGHT_SPEED_TRACK_OFF_BG),
        lv_color_hex(FAN_LIGHT_SPEED_TRACK_ON_BG),
        lv_color_hex(FAN_LIGHT_CONTROL_OFF_COLOR),
        lv_color_hex(FAN_LIGHT_CONTROL_ON_COLOR),
        lv_color_hex(0xE2E4E8),
        lv_color_hex(0xDEDEDE));
    ha_control_toggle_set_active_color(instance->control,
                                       lv_color_hex(FAN_CONTROL_ON_COLOR));
    ha_control_toggle_set_dark_disabled_colors(
        instance->control,
        lv_color_hex(FAN_UNAVAILABLE_SURFACE_DARK),
        lv_color_hex(0x454545));
    ha_control_toggle_set_disabled(instance->control, true);
    ha_control_toggle_set_change_cb(instance->control, toggle_changed,
                                    instance);

    lv_obj_t *knob = ha_control_toggle_get_knob(instance->control);
    if (knob) {
        ha_control_toggle_set_icon_text(instance->control, MDI_SYMBOL_FAN);
    }
    return ESP_OK;
}

static esp_err_t fan_create_speed_slider(entity_feature_instance_t *instance,
                                         fan_feature_data_t *data)
{
    lv_obj_t *host = instance->host;
    lv_obj_set_layout(host, LV_LAYOUT_NONE);

    instance->control = ha_control_slider_create(host);
    if (!instance->control) return ESP_ERR_NO_MEM;

    ha_control_slider_set_disabled(instance->control, true);
    ha_control_slider_set_change_cb(instance->control,
                                    fan_speed_slider_changed, instance);
    /* Restore the hand-drawn bar's strictly-percentage fill: a low speed
     * value shrinks the fill to a tiny stub instead of keeping the fixed
     * 24px end-cap shared by the brightness/cover/light sliders. */
    ha_control_slider_set_linear_fill(instance->control, true);
    data->touch_slider = instance->control;

    fan_update_speed_slider_visual(instance, data);
    return ESP_OK;
}

static esp_err_t fan_create_speed_options(entity_feature_instance_t *instance,
                                          fan_feature_data_t *data,
                                          int total_options)
{
    lv_obj_t *host = instance->host;
    lv_obj_set_layout(host, LV_LAYOUT_NONE);

    lv_obj_t *track = lv_obj_create(host);
    lv_obj_remove_style_all(track);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(track, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_radius(track, FAN_SPEED_OPTION_RADIUS, 0);
    lv_obj_set_style_clip_corner(track, true, 0);
    data->track = track;

    lv_obj_set_layout(track, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(track, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(track, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(track, FAN_SPEED_OPTION_GAP, 0);
    lv_obj_set_style_pad_all(track, 0, 0);

    for (int i = 0; i < total_options; i++) {
        lv_obj_t *opt = lv_obj_create(track);
        lv_obj_remove_style_all(opt);
        lv_obj_remove_flag(opt, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(opt, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_grow(opt, 1);
        lv_obj_set_height(opt, LV_PCT(100));
        lv_obj_set_style_radius(opt, FAN_SPEED_OPTION_RADIUS, 0);
        lv_obj_set_user_data(opt, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(opt, fan_speed_option_click,
                            LV_EVENT_CLICKED, data);
        data->options[i] = opt;

        lv_obj_t *icon = lv_label_create(opt);
        lv_label_set_text(icon, fan_speed_icon(i, total_options));
        if (fan_speed_option_is_numeric(i, total_options)) {
            lv_obj_set_style_text_font(icon, &ha_font_misan_regular_20, 0);
        } else {
            lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
        }
        lv_obj_set_style_text_color(icon, lv_color_hex(fan_inactive_fg()), 0);
        lv_obj_center(icon);
        data->option_icons[i] = icon;
    }
    return ESP_OK;
}

static esp_err_t fan_create_speed(entity_feature_instance_t *instance)
{
    fan_feature_data_t *data = lv_calloc(1, sizeof(*data));
    if (!data) return ESP_ERR_NO_MEM;
    data->type = instance->config->type;
    if (instance->entity_id) {
        lv_strlcpy(data->entity_id, instance->entity_id, sizeof(data->entity_id));
    }

    instance->control = instance->host;
    lv_obj_set_user_data(instance->host, data);
    lv_obj_add_event_cb(instance->host, fan_speed_delete_cb,
                        LV_EVENT_DELETE, data);

    return ESP_OK;
}

static esp_err_t fan_create_select_icons(entity_feature_instance_t *instance)
{
    instance->control = ha_control_select_create(instance->host);
    if (!instance->control) return ESP_ERR_NO_MEM;
    ha_control_select_set_surface_style(instance->control,
                                         lv_color_hex(fan_select_surface_color()),
                                         LV_OPA_100);
    ha_control_select_set_height(instance->control, 56);
    ha_control_select_set_hide_option_label(instance->control, true);
    ha_control_select_set_symbol_font(instance->control, &ha_font_mdi_20);
    ha_control_select_set_text_color(instance->control,
                                      lv_color_hex(fan_light_theme()
                                          ? 0x141414 : 0xE1E1E1));
    ha_control_select_set_disabled(instance->control, true);
    lv_obj_remove_flag(instance->control, LV_OBJ_FLAG_SCROLLABLE |
                                          LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(instance->control, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(instance->control, stop_feature_event,
                        LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(instance->control, fan_option_changed,
                        LV_EVENT_VALUE_CHANGED, instance);
    return ESP_OK;
}

static esp_err_t fan_create_select_dropdown(entity_feature_instance_t *instance,
                                            const char *title,
                                            const char *icon_src)
{
    instance->control = ha_control_select_menu_create(instance->host);
    if (!instance->control) return ESP_ERR_NO_MEM;
    ha_control_select_menu_set_title(instance->control, title);
    ha_control_select_menu_set_icon(instance->control, icon_src);
    ha_control_select_menu_set_disabled(instance->control, true);
    ha_control_select_menu_set_change_cb(instance->control,
                                          fan_menu_option_changed, instance);
    return ESP_OK;
}

static esp_err_t fan_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id || !instance->config ||
        !fan_feature_supports_config(instance->config)) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (instance->config->type) {
    case SCREEN_FEATURE_TOGGLE:
        return fan_create_toggle(instance);
    case SCREEN_FEATURE_FAN_SPEED:
        return fan_create_speed(instance);
    case SCREEN_FEATURE_FAN_PRESET_MODES:
        if (fan_feature_is_dropdown(instance->config)) {
            return fan_create_select_dropdown(instance,
                                              "",
                                              MDI_SYMBOL_TUNE_VARIANT);
        }
        return fan_create_select_icons(instance);
    case SCREEN_FEATURE_FAN_DIRECTION:
        return fan_create_select_icons(instance);
    case SCREEN_FEATURE_FAN_OSCILLATION:
        return fan_create_select_icons(instance);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static void fan_update_toggle(entity_feature_instance_t *instance,
                              const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) return;
    const bool available = snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool on = available && strcmp(snapshot->state, "on") == 0;

    ha_control_toggle_set_active_color(instance->control,
                                       lv_color_hex(fan_toggle_on_knob()));
    ha_control_toggle_set_value(instance->control, on, LV_ANIM_ON);
    ha_control_toggle_set_disabled(instance->control, !available);

    lv_obj_t *knob = ha_control_toggle_get_knob(instance->control);
    if (knob) {
        ha_control_toggle_set_icon_text(instance->control,
                                        on ? MDI_SYMBOL_FAN
                                           : MDI_SYMBOL_FAN_OFF);
        lv_obj_t *icon = lv_obj_get_child(knob, 0);
        if (icon) {
            lv_obj_set_style_text_color(
                icon,
                lv_color_hex(!available && !fan_light_theme()
                                 ? FAN_UNAVAILABLE_ICON_DARK
                                 : 0xFFFFFF),
                0);
        }
    }
}

static void fan_populate_speed_state(ha_entity_store_t *store,
                                     const char *entity_id,
                                     fan_feature_data_t *data)
{
    double num = 0;
    if (ha_entity_store_get_attr_number(store, entity_id,
                                        "percentage", &num) == ESP_OK) {
        data->percentage = (int)(num + 0.5);
    }
    bool has_step = false;
    if (ha_entity_store_get_attr_number(store, entity_id,
                                        "percentage_step", &num) == ESP_OK) {
        double step = num;
        if (step > 0.0) {
            data->speed_count = (int)(100.0 / step + 0.5);
            has_step = true;
        }
    }
    if (!has_step) {
        data->speed_count = FAN_HA_SPEED_COUNT_MAX;
    }
    if (data->speed_count <= 0) data->speed_count = 1;
    if (data->speed_count > FAN_HA_SPEED_COUNT_MAX) {
        data->speed_count = FAN_HA_SPEED_COUNT_MAX;
    }
}

static void fan_update_speed_slider_state(entity_feature_instance_t *instance,
                                          fan_feature_data_t *data)
{
    fan_update_speed_slider_visual(instance, data);
}

static void fan_update_speed(entity_feature_instance_t *instance,
                             ha_entity_store_t *store,
                             const ha_entity_snapshot_t *snapshot)
{
    fan_feature_data_t *data = get_data(instance->host);
    if (!data) return;

    data->available = store && snapshot &&
                      !entity_handler_snapshot_is_unavailable(snapshot);
    fan_populate_speed_state(store, instance->entity_id, data);
    data->on = data->available && strcmp(snapshot->state, "on") == 0;
    if (!data->on) data->percentage = 0;

    fan_speed_style_t auto_style;
    if (data->speed_count <= FAN_HA_SPEED_SWITCH_MAX) {
        auto_style = FAN_SPEED_STYLE_SWITCH;
    } else if (data->speed_count <= FAN_HA_SPEED_STEPS_MAX) {
        auto_style = FAN_SPEED_STYLE_STEPS;
    } else {
        auto_style = FAN_SPEED_STYLE_SLIDER;
    }

    if (auto_style != data->speed_style) {
        lv_obj_clean(instance->host);
        instance->control = NULL;
        data->track = NULL;
        data->touch_slider = NULL;
        data->speed_switch = NULL;
        memset(data->options, 0, sizeof(data->options));
        memset(data->option_icons, 0, sizeof(data->option_icons));
        data->speed_style = auto_style;
    }

    switch (auto_style) {
    case FAN_SPEED_STYLE_SLIDER:
        if (!data->touch_slider) {
            fan_create_speed_slider(instance, data);
        }
        fan_update_speed_slider_state(instance, data);
        break;
    case FAN_SPEED_STYLE_SWITCH:
        if (!data->speed_switch) {
            fan_create_select_icons(instance);
            data->speed_switch = instance->control;
        }
        fan_update_speed_switch(instance, data);
        break;
    case FAN_SPEED_STYLE_STEPS: {
        int total = data->speed_count + 1;
        if (total > FAN_HA_SPEED_COUNT_MAX + 1) total = FAN_HA_SPEED_COUNT_MAX + 1;
        if (!data->options[0] || total != data->steps_total) {
            lv_obj_clean(instance->host);
            instance->control = NULL;
            data->track = NULL;
            data->touch_slider = NULL;
            data->speed_switch = NULL;
            memset(data->options, 0, sizeof(data->options));
            memset(data->option_icons, 0, sizeof(data->option_icons));
            fan_create_speed_options(instance, data, total);
        }
        data->steps_total = total;
        fan_update_speed_steps_visual(data);
        break;
    }
    }
}

static void fan_update_select(entity_feature_instance_t *instance,
                              ha_entity_store_t *store,
                              const ha_entity_snapshot_t *snapshot,
                              const char *attr_key,
                              const char *local_key,
                              const char *const options[],
                              int option_count,
                              const char *const values[],
                              const char *const icons[])
{
    if (!instance || !instance->control) return;
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool entity_on = available && strcmp(snapshot->state, "on") == 0;

    ha_control_select_option_t icon_opts[FAN_SELECT_MAX_OPTIONS] = {0};
    uint32_t count = 0;
    for (int i = 0; i < option_count && count < FAN_SELECT_MAX_OPTIONS; i++) {
        icon_opts[count] = (ha_control_select_option_t){
            .value = values[i],
            .label = options[i],
            .symbol = icons[i],
        };
        count++;
    }
    ha_control_select_set_options_array(instance->control, icon_opts, count);

    /* The select widget defaults to its first option when its value is empty.
     * Start from HA's attribute each update so a missing direction stays
     * unselected instead of inheriting that visual default. */
    char selected[64] = "";
    if (store && attr_key) {
        char current[64] = "";
        if (ha_entity_store_get_attr_string(store, instance->entity_id,
                                            attr_key, current,
                                            sizeof(current)) == ESP_OK) {
            lv_strlcpy(selected, current, sizeof(selected));
        }
    }
    ha_control_select_set_value(instance->control, selected);
    ha_control_select_set_color(instance->control,
                                lv_color_hex(fan_select_selected_color(entity_on, available)));
    ha_control_select_set_disabled(instance->control, !available ||
                                   ha_control_select_get_option_count(
                                       instance->control) == 0);
    fan_apply_unavailable_icons_style(instance->control, available);
}

static void fan_update_select_preset(entity_feature_instance_t *instance,
                                     ha_entity_store_t *store,
                                     const ha_entity_snapshot_t *snapshot,
                                     bool dropdown)
{
    if (!instance || !instance->control) return;
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool entity_on = available && strcmp(snapshot->state, "on") == 0;

    cJSON *modes = NULL;
    if (store && ha_entity_store_dup_attr_json(store, instance->entity_id,
                                               "preset_modes", &modes) == ESP_OK &&
        cJSON_IsArray(modes)) {
        if (dropdown) {
            ha_control_select_menu_option_t menu_opts[FAN_SELECT_MAX_OPTIONS] = {0};
            uint32_t count = 0;
            const cJSON *mode = NULL;
            cJSON_ArrayForEach(mode, modes) {
                if (!cJSON_IsString(mode) || !mode->valuestring ||
                    !mode->valuestring[0] ||
                    strlen(mode->valuestring) >= HA_CONTROL_SELECT_MENU_MAX_VALUE_LEN)
                    continue;
                if (count == FAN_SELECT_MAX_OPTIONS) break;
                menu_opts[count] = (ha_control_select_menu_option_t){
                    .value = mode->valuestring,
                    .label = mode->valuestring,
                    .symbol = MDI_SYMBOL_CIRCLE_MEDIUM,
                };
                count++;
            }
            ha_control_select_menu_set_options(instance->control, menu_opts, count);
            cJSON_Delete(modes);
        } else {
            ha_control_select_option_t icon_opts[FAN_SELECT_MAX_OPTIONS] = {0};
            uint32_t count = 0;
            const cJSON *mode = NULL;
            cJSON_ArrayForEach(mode, modes) {
                if (!cJSON_IsString(mode) || !mode->valuestring ||
                    !mode->valuestring[0] ||
                    strlen(mode->valuestring) >= HA_CONTROL_SELECT_MAX_VALUE_LEN)
                    continue;
                if (count == FAN_SELECT_MAX_OPTIONS) break;
                icon_opts[count] = (ha_control_select_option_t){
                    .value = mode->valuestring,
                    .label = mode->valuestring,
                    .symbol = MDI_SYMBOL_CIRCLE_MEDIUM,
                };
                count++;
            }
            ha_control_select_set_options_array(instance->control, icon_opts, count);
            cJSON_Delete(modes);
        }
    } else if (available) {
        cJSON_Delete(modes);
        if (dropdown) {
            ha_control_select_menu_set_options(instance->control, NULL, 0);
        } else {
            ha_control_select_set_options_array(instance->control, NULL, 0);
        }
    } else {
        cJSON_Delete(modes);
    }

    char selected[64] = "";
    if (dropdown) {
        lv_strlcpy(selected,
                   ha_control_select_menu_get_value(instance->control),
                   sizeof(selected));
        if (store) {
            char current[64] = "";
            if (ha_entity_store_get_attr_string(store, instance->entity_id,
                                                "preset_mode", current,
                                                sizeof(current)) == ESP_OK) {
                lv_strlcpy(selected, current, sizeof(selected));
            }
        }
    } else {
        lv_strlcpy(selected, ha_control_select_get_value(instance->control),
                   sizeof(selected));
        if (store) {
            char current[64] = "";
            if (ha_entity_store_get_attr_string(store, instance->entity_id,
                                                "preset_mode", current,
                                                sizeof(current)) == ESP_OK) {
                lv_strlcpy(selected, current, sizeof(selected));
            }
        }
    }
    if (dropdown) {
        ha_control_select_menu_set_value(instance->control, selected);
        ha_control_select_menu_set_icon(
            instance->control,
            selected[0] ? MDI_SYMBOL_CIRCLE_MEDIUM
                        : MDI_SYMBOL_TUNE_VARIANT);
        ha_control_select_menu_set_disabled(instance->control, !available ||
            ha_control_select_menu_get_option_count(instance->control) == 0);
        fan_apply_dropdown_availability_style(instance->control, available);
    } else {
        ha_control_select_set_value(instance->control, selected);
        ha_control_select_set_color(instance->control,
                                    lv_color_hex(fan_select_selected_color(entity_on, available)));
        ha_control_select_set_disabled(instance->control, !available ||
            ha_control_select_get_option_count(instance->control) == 0);
        fan_apply_unavailable_icons_style(instance->control, available);
    }
}

static void fan_update_select_dir(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    const char *dir_options[] = {i18n_get(I18N_FAN_FORWARD), i18n_get(I18N_FAN_REVERSE)};
    const char *dir_values[] = {"forward", "reverse"};
    const char *dir_icons[] = {MDI_SYMBOL_ROTATE_RIGHT, MDI_SYMBOL_ROTATE_LEFT};
    fan_update_select(instance, store, snapshot, "direction", NULL,
                      dir_options, 2, dir_values, dir_icons);
}

static void fan_update_select_osc(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    const char *osc_options[] = {i18n_get(I18N_STATE_OFF), i18n_get(I18N_STATE_ON)};
    const char *osc_icons[] = {MDI_SYMBOL_ARROW_OSCILLATING_OFF,
                               MDI_SYMBOL_ARROW_OSCILLATING};
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);

    ha_control_select_option_t icon_opts[2] = {0};
    icon_opts[0] = (ha_control_select_option_t){
        .value = "off", .label = osc_options[0], .symbol = osc_icons[0]};
    icon_opts[1] = (ha_control_select_option_t){
        .value = "on", .label = osc_options[1], .symbol = osc_icons[1]};
    ha_control_select_set_options_array(instance->control, icon_opts, 2);

    bool oscillating = false;
    char selected[8] = "";
    lv_strlcpy(selected, ha_control_select_get_value(instance->control),
               sizeof(selected));
    if (store) {
        if (ha_entity_store_get_attr_bool(store, instance->entity_id,
                                          "oscillating", &oscillating) == ESP_OK) {
            lv_strlcpy(selected, oscillating ? "on" : "off", sizeof(selected));
        }
    }
    const bool entity_on = available && strcmp(snapshot->state, "on") == 0;
    ha_control_select_set_value(instance->control, selected);
    ha_control_select_set_color(instance->control,
                                lv_color_hex(fan_select_selected_color(entity_on, available)));
    ha_control_select_set_disabled(instance->control, !available ||
        ha_control_select_get_option_count(instance->control) == 0);
    fan_apply_unavailable_icons_style(instance->control, available);
}

static void fan_feature_update(entity_feature_instance_t *instance,
                               ha_entity_store_t *store,
                               const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->config) return;

    switch (instance->config->type) {
    case SCREEN_FEATURE_TOGGLE:
        fan_update_toggle(instance, snapshot);
        break;
    case SCREEN_FEATURE_FAN_SPEED:
        fan_update_speed(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_FAN_PRESET_MODES:
        fan_update_select_preset(instance, store, snapshot,
                                 fan_feature_is_dropdown(instance->config));
        break;
    case SCREEN_FEATURE_FAN_DIRECTION:
        fan_update_select_dir(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_FAN_OSCILLATION:
        fan_update_select_osc(instance, store, snapshot);
        break;
    default:
        break;
    }
}

static void fan_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (!instance || !instance->config) return;

    switch (instance->config->type) {
    case SCREEN_FEATURE_TOGGLE:
        if (instance->control) {
            ha_control_toggle_refresh_theme(instance->control);
            lv_obj_t *knob = ha_control_toggle_get_knob(instance->control);
            lv_obj_t *icon = knob ? lv_obj_get_child(knob, 0) : NULL;
            if (icon) {
                const bool unavailable =
                    ha_control_toggle_get_disabled(instance->control);
                lv_obj_set_style_text_color(
                    icon,
                    lv_color_hex(unavailable && !fan_light_theme()
                                     ? FAN_UNAVAILABLE_ICON_DARK
                                     : 0xFFFFFF),
                    0);
            }
        }
        break;
    case SCREEN_FEATURE_FAN_SPEED: {
        fan_feature_data_t *data = get_data(instance->host);
        if (!data) break;
        if (data->speed_style == FAN_SPEED_STYLE_SLIDER &&
            instance->control) {
            fan_update_speed_slider_visual(instance, data);
        } else if (data->speed_style == FAN_SPEED_STYLE_SWITCH &&
                   data->speed_switch) {
            fan_update_speed_switch(instance, data);
        } else if (data->speed_style == FAN_SPEED_STYLE_STEPS) {
            fan_update_speed_steps_visual(data);
        }
        break;
    }
    case SCREEN_FEATURE_FAN_DIRECTION:
    case SCREEN_FEATURE_FAN_OSCILLATION:
        if (instance->control) {
            lv_color_t surface = lv_color_hex(fan_select_surface_color());
            lv_color_t unselected = lv_color_hex(fan_light_theme()
                ? 0x141414 : 0xE1E1E1);
            ha_control_select_set_surface_style(instance->control, surface,
                                                LV_OPA_100);
            ha_control_select_set_text_color(instance->control, unselected);
            fan_apply_unavailable_icons_style(
                instance->control,
                !ha_control_select_get_disabled(instance->control));
        }
        break;
    case SCREEN_FEATURE_FAN_PRESET_MODES:
        if (fan_feature_is_dropdown(instance->config)) {
            if (instance->control) {
                ha_control_select_menu_apply_theme(instance->control);
            }
        } else if (instance->control) {
            lv_color_t surface = lv_color_hex(fan_select_surface_color());
            lv_color_t unselected = lv_color_hex(fan_light_theme()
                ? 0x141414 : 0xE1E1E1);
            ha_control_select_set_surface_style(instance->control, surface,
                                                LV_OPA_100);
            ha_control_select_set_text_color(instance->control, unselected);
            fan_apply_unavailable_icons_style(
                instance->control,
                !ha_control_select_get_disabled(instance->control));
        }
        break;
    default:
        break;
    }
}

const entity_feature_handler_t fan_feature_handler = {
    .supports = fan_feature_supports,
    .supports_config = fan_feature_supports_config,
    .create = fan_feature_create,
    .update = fan_feature_update,
    .apply_theme = fan_feature_apply_theme,
};
