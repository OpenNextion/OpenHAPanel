#include "cover_feature_handler.h"
#include "cover_favorite_positions_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "common/ha_control_select/ha_control_select.h"
#include "common/ha_control_slider/ha_control_slider.h"
#include "esp_log.h"
#include "ha_ui_font.h"
#include "screen_theme.h"

static const char *TAG = "cover_feature_handler";

#define COVER_FEATURE_OPEN          (1U << 0)
#define COVER_FEATURE_CLOSE         (1U << 1)
#define COVER_FEATURE_SET_POSITION  (1U << 2)
#define COVER_FEATURE_STOP          (1U << 3)

#define COVER_FEATURE_BUTTON_RADIUS      20
#define COVER_FEATURE_BUTTON_GAP         8

#define COVER_FEATURE_CONTROL_AVAILABLE_BG 0x242424
#define COVER_FEATURE_CONTROL_AVAILABLE_LIGHT 0xE3E3E3
#define COVER_FEATURE_CONTROL_PRESSED    0x3A3A3A
#define COVER_FEATURE_CONTROL_PRESSED_LIGHT 0xE0E0E0
#define COVER_FEATURE_DISABLED_BG        0x999999
#define COVER_FEATURE_ICON_FG_DISABLED   0x8F8F8F
#define COVER_FEATURE_LIGHT_TEXT_PRIMARY 0x141414
#define COVER_FEATURE_LIGHT_DISABLED_FG  0xBDBDBD
#define COVER_FEATURE_ICON_DISABLED_LIGHT 0xBDBDBD
#define COVER_FEATURE_ICON_DISABLED_DARK  0x6F6F6F

#define COVER_FEATURE_POSITION_ACTIVE_DARK        0x9065CC
#define COVER_FEATURE_POSITION_ACTIVE_LIGHT       0x926BC7
#define COVER_FEATURE_POSITION_DISABLED_DARK      0x6F6F6F
#define COVER_FEATURE_POSITION_DISABLED_LIGHT     0xBDBDBD
#define COVER_FEATURE_POSITION_MIN_PERCENT        4

#define COVER_FEATURE_FAVORITE_CLOSED_BG          0x9E9E9E
#define COVER_FEATURE_FAVORITE_OPEN_BG            0x926BC7
#define COVER_FEATURE_FAVORITE_TEXT_LIGHT         0x141414
#define COVER_FEATURE_FAVORITE_TEXT_DARK          0xFFFFFF
#define COVER_FEATURE_FAVORITE_TEXT_SELECTED       0xFFFFFF
#define COVER_FEATURE_FAVORITE_TRACK_LIGHT         0xE0E0E0
#define COVER_FEATURE_FAVORITE_TRACK_DARK          0x242424
#define COVER_FEATURE_FAVORITE_SELECTED_DARK       0x464646
#define COVER_FEATURE_FAVORITE_SELECTED_LIGHT      0xBDBDBD
#define COVER_FEATURE_FAVORITE_TEXT_DISABLED_LIGHT 0xBDBDBD
#define COVER_FEATURE_FAVORITE_TEXT_DISABLED_DARK  0x464646

typedef enum {
    COVER_ACTION_OPEN = 0,
    COVER_ACTION_STOP,
    COVER_ACTION_CLOSE,
} cover_action_t;

typedef struct {
    lv_obj_t *buttons[3];
    lv_obj_t *icons[3];
    bool enabled[3];
    bool available;
    bool horizontal;
} cover_open_close_data_t;

static void cover_favorite_set_options(lv_obj_t *control,
                                       const char *entity_id)
{
    uint8_t positions[COVER_FAVORITE_POSITION_MAX];
    size_t count = cover_favorite_positions_store_resolve(
                       entity_id, positions, COVER_FAVORITE_POSITION_MAX);
    if (count == 0) {
        ha_control_select_set_options_array(control, NULL, 0);
        return;
    }

    ha_control_select_option_t options[COVER_FAVORITE_POSITION_MAX] = {0};
    char values[COVER_FAVORITE_POSITION_MAX][8];
    char labels[COVER_FAVORITE_POSITION_MAX][8];
    for (size_t i = 0; i < count; i++) {
        snprintf(values[i], sizeof(values[i]), "%d", positions[i]);
        snprintf(labels[i], sizeof(labels[i]), "%d%%", positions[i]);
        options[i].value = values[i];
        options[i].label = labels[i];
        options[i].symbol = "";
    }
    ha_control_select_set_options_array(control, options, (uint32_t)count);
}

static bool cover_feature_light_theme(void)
{
    return screen_theme_get() == SCREEN_THEME_LIGHT;
}

static uint32_t cover_feature_get_features(ha_entity_store_t *store,
                                           const char *entity_id)
{
    double value = 0;
    if (!store || !entity_id ||
        ha_entity_store_get_attr_number(store, entity_id,
                                        "supported_features", &value) != ESP_OK ||
        value < 0) {
        return 0;
    }
    return (uint32_t)value;
}

static bool cover_feature_get_assumed_state(ha_entity_store_t *store,
                                            const char *entity_id)
{
    bool assumed = false;
    return store && entity_id &&
           ha_entity_store_get_attr_bool(store, entity_id,
                                         "assumed_state", &assumed) == ESP_OK &&
           assumed;
}

static bool cover_feature_get_position(ha_entity_store_t *store,
                                       const char *entity_id,
                                       int *position_out)
{
    double value = 0;
    if (!store || !entity_id || !position_out ||
        ha_entity_store_get_attr_number(store, entity_id,
                                        "current_position", &value) != ESP_OK) {
        return false;
    }
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    *position_out = (int)(value + 0.5);
    return true;
}

static bool cover_feature_is_horizontal(const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot) {
        return false;
    }
    return strcmp(snapshot->device_class, "awning") == 0 ||
           strcmp(snapshot->device_class, "curtain") == 0 ||
           strcmp(snapshot->device_class, "door") == 0 ||
           strcmp(snapshot->device_class, "gate") == 0;
}

static void cover_feature_call_service(const char *entity_id,
                                       const char *service,
                                       int position)
{
    if (!entity_id || !entity_id[0] || !service) {
        return;
    }

    cJSON *data = NULL;
    if (position >= 0) {
        data = cJSON_CreateObject();
        if (!data || !cJSON_AddNumberToObject(data, "position", position)) {
            cJSON_Delete(data);
            return;
        }
    }

    esp_err_t err = app_ha_call_entity_service_data("cover", service,
                                                    entity_id, data);
    cJSON_Delete(data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cover.%s for %s failed: %s",
                 service, entity_id, esp_err_to_name(err));
    }
}

static bool cover_feature_action_enabled(cover_action_t action,
                                         const char *state,
                                         bool assumed,
                                         bool has_position,
                                         int position)
{
    if (action == COVER_ACTION_STOP) {
        return true;
    }
    if (assumed) {
        return true;
    }
    if (action == COVER_ACTION_OPEN) {
        return strcmp(state, "opening") != 0 &&
               !(has_position ? position >= 100
                              : strcmp(state, "open") == 0);
    }
    return strcmp(state, "closing") != 0 &&
           !(has_position ? position <= 0
                          : strcmp(state, "closed") == 0);
}

static void stop_feature_event(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
}

static const char *cover_open_close_icon(cover_action_t action,
                                         bool horizontal)
{
    switch (action) {
    case COVER_ACTION_OPEN:
        return horizontal ? MDI_SYMBOL_ARROW_EXPAND_HORIZONTAL
                          : MDI_SYMBOL_ARROW_UP;
    case COVER_ACTION_CLOSE:
        return horizontal ? MDI_SYMBOL_ARROW_COLLAPSE_HORIZONTAL
                          : MDI_SYMBOL_ARROW_DOWN;
    case COVER_ACTION_STOP:
    default:
        return MDI_SYMBOL_STOP;
    }
}

static void cover_open_close_style_button(cover_open_close_data_t *data,
                                          cover_action_t action,
                                          bool enabled,
                                          bool horizontal)
{
    lv_obj_t *button = data->buttons[action];
    lv_obj_t *icon = data->icons[action];
    if (!button || !icon) {
        return;
    }
    const bool light = cover_feature_light_theme();
    const uint32_t available_bg =
        light ? COVER_FEATURE_CONTROL_AVAILABLE_LIGHT
              : COVER_FEATURE_CONTROL_AVAILABLE_BG;
    const uint32_t button_bg = available_bg;

    lv_obj_set_style_bg_color(button,
        lv_color_hex(button_bg),
        0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button,
        lv_color_hex(light ? COVER_FEATURE_CONTROL_PRESSED_LIGHT
                           : COVER_FEATURE_CONTROL_PRESSED),
        LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    /* Reapply state styles for both themes so a theme switch cannot leave a
     * stale disabled background from the previous palette. */
    lv_obj_set_style_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_opa(button, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(button, lv_color_hex(button_bg),
                              LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_DISABLED);
    /* LVGL's default disabled style recolors buttons by 50%; disable that
     * overlay so the configured theme background remains the rendered color. */
    lv_obj_set_style_recolor_opa(button, LV_OPA_TRANSP,
                                  LV_STATE_DISABLED);

    const uint32_t icon_disabled =
        light ? COVER_FEATURE_ICON_DISABLED_LIGHT
              : COVER_FEATURE_ICON_DISABLED_DARK;
    lv_label_set_text(icon, cover_open_close_icon(action, horizontal));
    lv_obj_set_style_text_color(icon,
        lv_color_hex(enabled
            ? (light ? COVER_FEATURE_LIGHT_TEXT_PRIMARY : 0xE1E1E1)
            : icon_disabled),
        0);
    lv_obj_set_style_text_color(icon,
        lv_color_hex(icon_disabled),
        LV_STATE_DISABLED);

    if (enabled) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }
    data->enabled[action] = enabled;
}

static void cover_open_close_click(lv_event_t *event)
{
    entity_feature_instance_t *instance = lv_event_get_user_data(event);
    lv_obj_t *button = lv_event_get_current_target_obj(event);
    if (!instance || !instance->entity_id || !button) {
        return;
    }
    const cover_action_t action =
        (cover_action_t)(uintptr_t)lv_obj_get_user_data(button);
    switch (action) {
    case COVER_ACTION_OPEN:
        cover_feature_call_service(instance->entity_id, "open_cover", -1);
        break;
    case COVER_ACTION_STOP:
        cover_feature_call_service(instance->entity_id, "stop_cover", -1);
        break;
    case COVER_ACTION_CLOSE:
        cover_feature_call_service(instance->entity_id, "close_cover", -1);
        break;
    default:
        break;
    }
}

static void cover_open_close_delete_cb(lv_event_t *event)
{
    cover_open_close_data_t *data = lv_event_get_user_data(event);
    if (data) {
        lv_free(data);
    }
}

static esp_err_t cover_create_open_close(entity_feature_instance_t *instance)
{
    cover_open_close_data_t *data = lv_calloc(1, sizeof(*data));
    if (!data) {
        return ESP_ERR_NO_MEM;
    }

    lv_obj_t *group = lv_obj_create(instance->host);
    if (!group) {
        lv_free(data);
        return ESP_ERR_NO_MEM;
    }
    lv_obj_remove_style_all(group);
    lv_obj_remove_flag(group, LV_OBJ_FLAG_SCROLLABLE |
                              LV_OBJ_FLAG_CLICKABLE |
                              LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_size(group, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(group, 0, 0);
    lv_obj_set_style_pad_gap(group, COVER_FEATURE_BUTTON_GAP, 0);
    lv_obj_add_event_cb(group, stop_feature_event, LV_EVENT_ALL, NULL);

    static const cover_action_t order[] = {
        COVER_ACTION_OPEN, COVER_ACTION_STOP, COVER_ACTION_CLOSE};
    for (int i = 0; i < 3; i++) {
        const cover_action_t action = order[i];
        lv_obj_t *button = lv_button_create(group);
        lv_obj_set_user_data(button, (void *)(uintptr_t)action);
        lv_obj_set_flex_grow(button, 1);
        lv_obj_set_height(button, LV_PCT(100));
        lv_obj_set_style_radius(button, COVER_FEATURE_BUTTON_RADIUS, 0);
        lv_obj_set_style_radius(button, COVER_FEATURE_BUTTON_RADIUS,
                                LV_STATE_PRESSED);
        lv_obj_set_style_transform_width(button, 0, LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(button, 0, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_add_event_cb(button, cover_open_close_click,
                            LV_EVENT_CLICKED, instance);
        data->buttons[action] = button;

        lv_obj_t *icon = lv_label_create(button);
        lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
        lv_obj_center(icon);
        data->icons[action] = icon;
    }

    lv_obj_set_user_data(instance->host, data);
    lv_obj_add_event_cb(instance->host, cover_open_close_delete_cb,
                        LV_EVENT_DELETE, data);
    instance->control = group;

    for (int a = 0; a < 3; a++) {
        cover_open_close_style_button(data, (cover_action_t)a, false, false);
    }
    return ESP_OK;
}

static void cover_open_close_update(entity_feature_instance_t *instance,
                                    ha_entity_store_t *store,
                                    const ha_entity_snapshot_t *snapshot)
{
    cover_open_close_data_t *data = lv_obj_get_user_data(instance->host);
    if (!data) {
        return;
    }
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    data->available = available;
    const uint32_t features = cover_feature_get_features(store,
                                                         instance->entity_id);
    const bool assumed = cover_feature_get_assumed_state(store,
                                                         instance->entity_id);
    int position = 0;
    const bool has_position = cover_feature_get_position(store,
                                                         instance->entity_id,
                                                         &position);
    const char *state = snapshot ? snapshot->state : "";
    data->horizontal = cover_feature_is_horizontal(snapshot);

    for (int a = 0; a < 3; a++) {
        const cover_action_t action = (cover_action_t)a;
        const uint32_t bit = (action == COVER_ACTION_OPEN)
                                 ? COVER_FEATURE_OPEN
                                 : (action == COVER_ACTION_STOP)
                                       ? COVER_FEATURE_STOP
                                       : COVER_FEATURE_CLOSE;
        if (features != 0 && (features & bit) == 0) {
            lv_obj_add_flag(data->buttons[a], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(data->buttons[a], LV_OBJ_FLAG_HIDDEN);
        const bool enabled = available &&
                             cover_feature_action_enabled(action, state,
                                                          assumed, has_position,
                                                          position);
        cover_open_close_style_button(data, action, enabled,
                                      data->horizontal);
    }
}

static void cover_open_close_apply_theme(entity_feature_instance_t *instance)
{
    cover_open_close_data_t *data = lv_obj_get_user_data(instance->host);
    if (!data) {
        return;
    }
    for (int a = 0; a < 3; a++) {
        if (lv_obj_has_flag(data->buttons[a], LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        cover_open_close_style_button(data, (cover_action_t)a,
                                      data->enabled[a], data->horizontal);
    }
}

static uint32_t cover_feature_favorite_selected_color(bool available,
                                                       int position)
{
    if (!available) {
        return cover_feature_light_theme()
                   ? COVER_FEATURE_FAVORITE_SELECTED_LIGHT
                   : COVER_FEATURE_FAVORITE_SELECTED_DARK;
    }
    return position <= 0 ? COVER_FEATURE_FAVORITE_CLOSED_BG
                         : COVER_FEATURE_FAVORITE_OPEN_BG;
}

static void cover_favorite_apply_surface(lv_obj_t *control)
{
    const bool light = cover_feature_light_theme();
    ha_control_select_set_surface_style(control,
        lv_color_hex(light ? COVER_FEATURE_FAVORITE_TRACK_LIGHT
                           : COVER_FEATURE_FAVORITE_TRACK_DARK),
        LV_OPA_COVER);
    ha_control_select_set_text_color(control,
        lv_color_hex(light ? COVER_FEATURE_FAVORITE_TEXT_LIGHT
                           : COVER_FEATURE_FAVORITE_TEXT_DARK));
}

static void cover_favorite_apply_disabled_style(lv_obj_t *control,
                                                bool available)
{
    if (!control) {
        return;
    }

    if (available) {
        cover_favorite_apply_surface(control);
        return;
    }

    const bool light = cover_feature_light_theme();
    const lv_color_t disabled_text = lv_color_hex(
        light ? COVER_FEATURE_FAVORITE_TEXT_DISABLED_LIGHT
              : COVER_FEATURE_FAVORITE_TEXT_DISABLED_DARK);

    ha_control_select_set_surface_style(
        control,
        lv_color_hex(light ? 0xE3E3E3 : COVER_FEATURE_FAVORITE_TRACK_DARK),
        LV_OPA_COVER);

    /* Buttonmatrix items do not reliably expose the parent's DISABLED state
     * while drawing.  Apply the unavailable color to the base item selector,
     * then keep the checked item text white explicitly. */
    ha_control_select_set_text_color(control, disabled_text);
    lv_obj_set_style_text_color(
        control,
        lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(control, lv_color_white(),
                                LV_PART_ITEMS | LV_STATE_CHECKED |
                                    LV_STATE_PRESSED);
}

static void cover_favorite_changed(lv_event_t *event)
{
    entity_feature_instance_t *instance = lv_event_get_user_data(event);
    if (!instance || !instance->control || !instance->entity_id) {
        return;
    }
    const char *value = ha_control_select_get_value(instance->control);
    if (!value || !value[0]) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    if (!store) {
        return;
    }
    ha_entity_snapshot_t snapshot = {0};
    if (ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) {
        return;
    }
    int position = atoi(value);
    if (position < 0) {
        position = 0;
    } else if (position > 100) {
        position = 100;
    }
    cover_feature_call_service(instance->entity_id, "set_cover_position",
                               position);
}

static esp_err_t cover_create_favorite(entity_feature_instance_t *instance)
{
    instance->control = ha_control_select_create(instance->host);
    if (!instance->control) {
        return ESP_ERR_NO_MEM;
    }
    ha_control_select_set_height(instance->control, 56);
    ha_control_select_set_text_font(instance->control, ha_ui_font_regular(12));
    ha_control_select_set_visible_limit(instance->control, (uint32_t)instance->visible_limit);
    cover_favorite_set_options(instance->control, instance->entity_id);
    cover_favorite_apply_surface(instance->control);
    ha_control_select_set_disabled(instance->control, true);
    ha_control_select_set_disabled_opa(instance->control, LV_OPA_COVER);
    cover_favorite_apply_disabled_style(instance->control, false);
    lv_obj_remove_flag(instance->control, LV_OBJ_FLAG_SCROLLABLE |
                                          LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(instance->control, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(instance->control, stop_feature_event,
                        LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(instance->control, cover_favorite_changed,
                        LV_EVENT_VALUE_CHANGED, instance);
    return ESP_OK;
}

static void cover_favorite_update(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) {
        return;
    }
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);

    int position = -1;
    cover_feature_get_position(store, instance->entity_id, &position);

    char selected[16] = "";
    if (position >= 0) {
        snprintf(selected, sizeof(selected), "%d", position);
    }
    cover_favorite_set_options(instance->control, instance->entity_id);
    ha_control_select_set_visible_limit(instance->control, (uint32_t)instance->visible_limit);
    ha_control_select_set_text_font(instance->control, ha_ui_font_regular(12));
    ha_control_select_set_value(instance->control, selected);
    ha_control_select_set_color(instance->control,
        lv_color_hex(cover_feature_favorite_selected_color(available,
                                                           position)));
    ha_control_select_set_disabled(instance->control, !available);
    cover_favorite_apply_disabled_style(instance->control, available);
}

static void cover_favorite_apply_theme(entity_feature_instance_t *instance)
{
    if (instance && instance->control) {
        cover_favorite_apply_surface(instance->control);
        cover_favorite_apply_disabled_style(
            instance->control,
            !ha_control_select_get_disabled(instance->control));
    }
}

static void cover_position_changed(uint8_t percent,
                                   ha_control_slider_phase_t phase,
                                   void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id) {
        return;
    }
    ha_control_slider_set_power(instance->control, true);
    if (phase != HA_CONTROL_SLIDER_COMMIT) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    if (!store) {
        return;
    }
    ha_entity_snapshot_t snapshot = {0};
    if (ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) {
        return;
    }
    int position = 100 - (int)percent;
    if (position < 0) {
        position = 0;
    } else if (position > 100) {
        position = 100;
    }
    cover_feature_call_service(instance->entity_id, "set_cover_position",
                               position);
}

static esp_err_t cover_create_position(entity_feature_instance_t *instance)
{
    instance->control = ha_control_slider_create(instance->host);
    if (!instance->control) {
        return ESP_ERR_NO_MEM;
    }
    ha_control_slider_set_disabled(instance->control, true);
    ha_control_slider_set_change_cb(instance->control, cover_position_changed,
                                    instance);
    return ESP_OK;
}

static void cover_position_update(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) {
        return;
    }
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool light = cover_feature_light_theme();

    int position = 0;
    if (available) {
        cover_feature_get_position(store, instance->entity_id, &position);
    }
    const uint8_t slider_percent = (uint8_t)LV_CLAMP(
        COVER_FEATURE_POSITION_MIN_PERCENT, 100 - position, 100);
    ha_control_slider_set_value(instance->control,
                                slider_percent, LV_ANIM_OFF);
    ha_control_slider_set_power(instance->control, true);
    uint32_t active_color;
    if (available) {
        active_color = light ? COVER_FEATURE_POSITION_ACTIVE_LIGHT
                             : COVER_FEATURE_POSITION_ACTIVE_DARK;
    } else {
        active_color = light ? COVER_FEATURE_POSITION_DISABLED_LIGHT
                             : COVER_FEATURE_POSITION_DISABLED_DARK;
    }
    ha_control_slider_set_active_color_raw(instance->control,
        lv_color_hex(active_color));
    ha_control_slider_set_disabled(instance->control, !available);
}

static void cover_position_apply_theme(entity_feature_instance_t *instance)
{
    if (!instance || !instance->control) {
        return;
    }
    const bool light = cover_feature_light_theme();
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    const bool available = store && instance->entity_id &&
        ha_entity_store_get(store, instance->entity_id, &snapshot) == ESP_OK &&
        !entity_handler_snapshot_is_unavailable(&snapshot);
    ha_control_slider_set_active_color_raw(instance->control,
        lv_color_hex(available
                         ? (light ? COVER_FEATURE_POSITION_ACTIVE_LIGHT
                                  : COVER_FEATURE_POSITION_ACTIVE_DARK)
                         : (light ? COVER_FEATURE_POSITION_DISABLED_LIGHT
                                  : COVER_FEATURE_POSITION_DISABLED_DARK)));
}

static bool cover_feature_supports(ha_entity_store_t *store,
                                   const char *entity_id,
                                   screen_feature_type_t type)
{
    if (type != SCREEN_FEATURE_COVER_OPEN_CLOSE &&
        type != SCREEN_FEATURE_COVER_FAVORITE_POSITIONS &&
        type != SCREEN_FEATURE_COVER_POSITION) {
        return false;
    }
    if (!store || !entity_id) {
        return true;
    }

    double value = 0;
    if (ha_entity_store_get_attr_number(store, entity_id,
                                        "supported_features", &value) != ESP_OK ||
        value < 0) {
        return true;
    }
    const uint32_t features = (uint32_t)value;
    if (type == SCREEN_FEATURE_COVER_OPEN_CLOSE) {
        return (features & (COVER_FEATURE_OPEN | COVER_FEATURE_STOP |
                            COVER_FEATURE_CLOSE)) != 0;
    }
    if (type == SCREEN_FEATURE_COVER_FAVORITE_POSITIONS) {
        if ((features & COVER_FEATURE_SET_POSITION) == 0) {
            return false;
        }
        uint8_t positions[COVER_FAVORITE_POSITION_MAX];
        return cover_favorite_positions_store_resolve(
                   entity_id, positions, COVER_FAVORITE_POSITION_MAX) > 0;
    }
    return (features & COVER_FEATURE_SET_POSITION) != 0;
}

static bool cover_feature_supports_config(const screen_feature_config_t *config)
{
    if (!config) {
        return false;
    }
    switch (config->type) {
    case SCREEN_FEATURE_COVER_OPEN_CLOSE:
    case SCREEN_FEATURE_COVER_FAVORITE_POSITIONS:
    case SCREEN_FEATURE_COVER_POSITION:
        return true;
    default:
        return false;
    }
}

static esp_err_t cover_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config ||
        !cover_feature_supports_config(instance->config)) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (instance->config->type) {
    case SCREEN_FEATURE_COVER_OPEN_CLOSE:
        return cover_create_open_close(instance);
    case SCREEN_FEATURE_COVER_FAVORITE_POSITIONS:
        return cover_create_favorite(instance);
    case SCREEN_FEATURE_COVER_POSITION:
        return cover_create_position(instance);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static void cover_feature_update(entity_feature_instance_t *instance,
                                 ha_entity_store_t *store,
                                 const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->config) {
        return;
    }

    switch (instance->config->type) {
    case SCREEN_FEATURE_COVER_OPEN_CLOSE:
        cover_open_close_update(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_COVER_FAVORITE_POSITIONS:
        cover_favorite_update(instance, store, snapshot);
        break;
    case SCREEN_FEATURE_COVER_POSITION:
        cover_position_update(instance, store, snapshot);
        break;
    default:
        break;
    }
}

static void cover_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (!instance || !instance->config) {
        return;
    }

    switch (instance->config->type) {
    case SCREEN_FEATURE_COVER_OPEN_CLOSE:
        cover_open_close_apply_theme(instance);
        break;
    case SCREEN_FEATURE_COVER_FAVORITE_POSITIONS:
        cover_favorite_apply_theme(instance);
        break;
    case SCREEN_FEATURE_COVER_POSITION:
        cover_position_apply_theme(instance);
        break;
    default:
        break;
    }
}

const entity_feature_handler_t cover_feature_handler = {
    .supports = cover_feature_supports,
    .supports_config = cover_feature_supports_config,
    .create = cover_feature_create,
    .update = cover_feature_update,
    .apply_theme = cover_feature_apply_theme,
};
