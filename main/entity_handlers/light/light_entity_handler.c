#include "light_entity_handler.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "components/light_assumed_power_buttons.h"
#include "components/light_brightness_slider.h"
#include "components/light_color_temp_slider.h"
#include "components/light_effect_selector.h"
#include "components/light_mode_bar.h"
#include "components/light_power_toggle.h"
#include "components/light_preset_colors.h"
#include "components/light_rgb_picker.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_log.h"
#include "light_favorite_colors_store.h"
#include "screen/components/ha_relative_time_label.h"

#define LIGHT_EFFECT_MAX_COUNT 16
#define LIGHT_EFFECT_NAME_MAX  48
#define LIGHT_MIN_BRIGHTNESS_PERCENT 1
#define LIGHT_DEFAULT_BRIGHTNESS_PERCENT 70
#define LIGHT_CONTROL_OVERFLOW_SIZE 44
#define LIGHT_CONTENT_GAP 16
#define LIGHT_STATE_BLOCK_HEIGHT 54
#define LIGHT_CONTROL_AREA_HEIGHT 235
#define LIGHT_MODE_BAR_HEIGHT 48
#define LIGHT_PAGE_AREA_HEIGHT \
    (LIGHT_CONTROL_AREA_HEIGHT + LIGHT_CONTENT_GAP + LIGHT_MODE_BAR_HEIGHT)
#define LIGHT_SECOND_PAGE_TOP_OFFSET 8
#define LIGHT_SECOND_PAGE_GAP 31
#define LIGHT_FEATURE_EFFECT 4U
#define LIGHT_COLOR_TEMP_FAVORITE_COUNT 4
/* Let rapid taps settle before issuing a state-changing HA service call. */
#define LIGHT_PRESET_DEBOUNCE_MS 120

static const char *TAG = "light_entity_handler";

typedef struct {
    light_mode_t active_mode;
    bool active_mode_initialized;
    uint8_t last_brightness_percent;
    bool last_brightness_initialized;
    lv_color_t last_brightness_display_color;
    bool last_brightness_display_color_initialized;
    uint8_t tileview_page;
    lv_timer_t *preset_timer;
    bool has_pending_preset;
    light_favorite_color_t pending_preset;
    char preset_entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
} light_more_info_session_t;

typedef struct {
    bool supports_brightness;
    bool supports_color_temp;
    bool supports_color;
    bool supports_effects;
    bool supports_favorite_colors;
} light_capabilities_t;

typedef enum {
    LIGHT_COLOR_SOURCE_DEFAULT,
    LIGHT_COLOR_SOURCE_RGB,
    LIGHT_COLOR_SOURCE_COLOR_TEMP,
} light_color_source_t;

typedef struct {
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    bool available;
    bool on;
    bool state_unknown;
    bool assumed_state;
    bool has_rgb_color;
    light_capabilities_t capabilities;
    uint32_t mode_mask;
    light_mode_t active_mode;
    uint8_t brightness_percent;
    uint16_t min_kelvin;
    uint16_t max_kelvin;
    uint16_t kelvin;
    light_rgb_t rgb;
    bool has_custom_favorite_colors;
    light_favorite_color_t favorite_colors[LIGHT_FAVORITE_COLOR_MAX];
    size_t favorite_color_count;
    light_color_source_t color_source;
    char effects[LIGHT_EFFECT_MAX_COUNT][LIGHT_EFFECT_NAME_MAX];
    size_t effect_count;
    size_t selected_effect;
    lv_obj_t *state_label;
    lv_obj_t *control_host;
    lv_obj_t *mode_bar;
    lv_obj_t *effect_selector;
    lv_obj_t *power_toggle;
    lv_obj_t *assumed_power_buttons;
    lv_obj_t *tileview;
    lv_obj_t *tileview_pages[2];
    lv_obj_t *page_dots[2];
    light_more_info_session_t *session;
    entity_more_info_close_cb_t close;
    void *close_user_data;
} light_more_info_page_t;

static lv_color_t light_brightness_display_color(
    const light_more_info_page_t *page);
static void light_remember_brightness_display_color(
    light_more_info_page_t *page);
static void light_prepare_favorite_display_colors(
    light_more_info_page_t *page);

static void light_session_delete_cb(lv_event_t *event)
{
    lv_obj_t *screen = lv_event_get_target_obj(event);
    light_more_info_session_t *session = lv_event_get_user_data(event);
    if (screen && lv_obj_get_user_data(screen) == session) {
        lv_obj_set_user_data(screen, NULL);
    }
    if (session && session->preset_timer) {
        lv_timer_delete(session->preset_timer);
    }
    lv_free(session);
}

static light_more_info_session_t *light_get_session(lv_obj_t *screen)
{
    if (!screen) {
        return NULL;
    }
    /* The screen survives child cleanup during a More Info state refresh, so
     * its session keeps the user's selected UI mode across full rebuilds. */
    light_more_info_session_t *session = lv_obj_get_user_data(screen);
    if (session) {
        return session;
    }
    session = lv_calloc(1, sizeof(*session));
    if (!session) {
        return NULL;
    }
    lv_obj_set_user_data(screen, session);
    lv_obj_add_event_cb(screen,
                        light_session_delete_cb,
                        LV_EVENT_DELETE,
                        session);
    return session;
}

static bool light_color_mode_is_rgb(const char *mode)
{
    return mode &&
           (strcmp(mode, "hs") == 0 || strcmp(mode, "xy") == 0 ||
            strcmp(mode, "rgb") == 0 || strcmp(mode, "rgbw") == 0 ||
            strcmp(mode, "rgbww") == 0);
}

static bool light_color_mode_has_brightness(const char *mode)
{
    return mode &&
           (strcmp(mode, "brightness") == 0 ||
            strcmp(mode, "color_temp") == 0 ||
            strcmp(mode, "white") == 0 ||
            light_color_mode_is_rgb(mode));
}

static bool light_get_number(ha_entity_store_t *store,
                             const char *entity_id,
                             const char *attribute,
                             double *value)
{
    return ha_entity_store_get_attr_number(store,
                                           entity_id,
                                           attribute,
                                           value) == ESP_OK;
}

static void light_analyze_supported_modes(const cJSON *modes,
                                          light_capabilities_t *capabilities)
{
    if (!cJSON_IsArray(modes) || !capabilities) {
        return;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, modes) {
        if (!cJSON_IsString(item) || !item->valuestring) {
            continue;
        }
        if (light_color_mode_has_brightness(item->valuestring)) {
            capabilities->supports_brightness = true;
        }
        if (strcmp(item->valuestring, "color_temp") == 0) {
            capabilities->supports_color_temp = true;
        }
        if (light_color_mode_is_rgb(item->valuestring)) {
            capabilities->supports_color = true;
        }
    }
    capabilities->supports_favorite_colors =
        capabilities->supports_color || capabilities->supports_color_temp;
}

static void light_apply_capabilities(light_more_info_page_t *page,
                                     uint32_t supported_features)
{
    if (!page) {
        return;
    }
    page->capabilities.supports_effects =
        (supported_features & LIGHT_FEATURE_EFFECT) != 0 &&
        page->effect_count > 0;
    page->capabilities.supports_favorite_colors =
        page->capabilities.supports_color ||
        page->capabilities.supports_color_temp;

    page->mode_mask = 0;
    if (page->capabilities.supports_brightness) {
        page->mode_mask |= LIGHT_MODE_MASK_BRIGHTNESS;
    }
    if (page->capabilities.supports_color) {
        page->mode_mask |= LIGHT_MODE_MASK_RGB;
    }
    if (page->capabilities.supports_color_temp) {
        page->mode_mask |= LIGHT_MODE_MASK_COLOR_TEMP;
    }
}

static void light_parse_rgb(const cJSON *rgb, light_more_info_page_t *page)
{
    if (!cJSON_IsArray(rgb) || cJSON_GetArraySize(rgb) < 3 || !page) {
        return;
    }
    const cJSON *red = cJSON_GetArrayItem(rgb, 0);
    const cJSON *green = cJSON_GetArrayItem(rgb, 1);
    const cJSON *blue = cJSON_GetArrayItem(rgb, 2);
    if (!cJSON_IsNumber(red) || !cJSON_IsNumber(green) ||
        !cJSON_IsNumber(blue)) {
        return;
    }
    page->rgb.red = (uint8_t)LV_CLAMP(0, red->valueint, 255);
    page->rgb.green = (uint8_t)LV_CLAMP(0, green->valueint, 255);
    page->rgb.blue = (uint8_t)LV_CLAMP(0, blue->valueint, 255);
    page->has_rgb_color = true;
}

static void light_parse_effects(const cJSON *effects,
                                light_more_info_page_t *page)
{
    if (!cJSON_IsArray(effects) || !page) {
        return;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, effects) {
        if (page->effect_count >= LIGHT_EFFECT_MAX_COUNT) {
            break;
        }
        if (!cJSON_IsString(item) || !item->valuestring) {
            continue;
        }
        snprintf(page->effects[page->effect_count],
                 sizeof(page->effects[page->effect_count]),
                 "%s",
                 item->valuestring);
        page->effect_count++;
    }
}

static void light_read_page_state(const entity_more_info_context_t *context,
                                  light_more_info_page_t *page)
{
    snprintf(page->entity_id,
             sizeof(page->entity_id),
             "%s",
             context->snapshot.entity_id);
    page->available =
        !entity_handler_snapshot_is_unavailable(&context->snapshot);
    page->on = strcmp(context->snapshot.state, "on") == 0;
    page->state_unknown = strcmp(context->snapshot.state, "unknown") == 0;
    page->brightness_percent =
        page->session && page->session->last_brightness_initialized
            ? page->session->last_brightness_percent
            : LIGHT_DEFAULT_BRIGHTNESS_PERCENT;
    page->min_kelvin = 2000;
    page->max_kelvin = 6535;
    page->kelvin = 4000;
    page->rgb = (light_rgb_t) {255, 255, 255};
    page->color_source = LIGHT_COLOR_SOURCE_DEFAULT;
    page->active_mode = LIGHT_MODE_BRIGHTNESS;
    page->close = context->close;
    page->close_user_data = context->close_user_data;

    bool assumed_state = false;
    if (ha_entity_store_get_attr_bool(context->store,
                                      page->entity_id,
                                      "assumed_state",
                                      &assumed_state) == ESP_OK) {
        page->assumed_state = assumed_state;
    }

    double number = 0;
    if (light_get_number(context->store, page->entity_id, "brightness", &number)) {
        const int minimum_percent = page->on
                                        ? LIGHT_MIN_BRIGHTNESS_PERCENT
                                        : 0;
        page->brightness_percent = (uint8_t)LV_CLAMP(
            minimum_percent,
            (int)(number * 100.0 / 255.0 + 0.5),
            100);
        if (page->brightness_percent > 0 && page->session) {
            page->session->last_brightness_percent =
                page->brightness_percent;
            page->session->last_brightness_initialized = true;
        }
    }
    if (light_get_number(context->store,
                         page->entity_id,
                         "min_color_temp_kelvin",
                         &number)) {
        page->min_kelvin = (uint16_t)LV_CLAMP(1, (int)number, 65535);
    }
    if (light_get_number(context->store,
                         page->entity_id,
                         "max_color_temp_kelvin",
                         &number)) {
        page->max_kelvin = (uint16_t)LV_CLAMP(1, (int)number, 65535);
    }
    if (light_get_number(context->store,
                         page->entity_id,
                         "color_temp_kelvin",
                         &number)) {
        page->kelvin = (uint16_t)LV_CLAMP(page->min_kelvin,
                                         (int)number,
                                         page->max_kelvin);
    }

    cJSON *attribute = NULL;
    if (ha_entity_store_dup_attr_json(context->store,
                                      page->entity_id,
                                      "supported_color_modes",
                                      &attribute) == ESP_OK) {
        light_analyze_supported_modes(attribute, &page->capabilities);
        cJSON_Delete(attribute);
    }
    attribute = NULL;
    if (ha_entity_store_dup_attr_json(context->store,
                                      page->entity_id,
                                      "rgb_color",
                                      &attribute) == ESP_OK) {
        light_parse_rgb(attribute, page);
        cJSON_Delete(attribute);
    }
    attribute = NULL;
    if (ha_entity_store_dup_attr_json(context->store,
                                      page->entity_id,
                                      "effect_list",
                                      &attribute) == ESP_OK) {
        light_parse_effects(attribute, page);
        cJSON_Delete(attribute);
    }

    uint32_t supported_features = 0;
    if (light_get_number(context->store,
                         page->entity_id,
                         "supported_features",
                         &number)) {
        supported_features = number > 0 ? (uint32_t)number : 0;
    }
    light_apply_capabilities(page, supported_features);
    page->has_custom_favorite_colors = light_favorite_colors_store_get(
        page->entity_id,
        page->favorite_colors,
        LIGHT_FAVORITE_COLOR_MAX,
        &page->favorite_color_count);
    if (page->has_custom_favorite_colors) {
        page->capabilities.supports_favorite_colors =
            page->favorite_color_count > 0;
        light_prepare_favorite_display_colors(page);
    }
    if (page->state_unknown && page->mode_mask == 0) {
        /* Unknown is actionable only for the explicit on/off button layout.
         * Keep the previous unavailable behavior for adjustable controls. */
        page->available = !entity_handler_snapshot_is_service_unavailable(
            &context->snapshot);
    }

    char color_mode[24] = {0};
    const bool has_color_mode =
        ha_entity_store_get_attr_string(context->store,
                                        page->entity_id,
                                        "color_mode",
                                        color_mode,
                                        sizeof(color_mode)) == ESP_OK &&
        color_mode[0] != '\0';
    if (has_color_mode) {
        if (strcmp(color_mode, "color_temp") == 0 &&
            (page->mode_mask & LIGHT_MODE_MASK_COLOR_TEMP)) {
            page->active_mode = LIGHT_MODE_COLOR_TEMP;
            page->color_source = LIGHT_COLOR_SOURCE_COLOR_TEMP;
        }
        else if (light_color_mode_is_rgb(color_mode) &&
                 (page->mode_mask & LIGHT_MODE_MASK_RGB)) {
            page->active_mode = LIGHT_MODE_RGB;
            page->color_source = LIGHT_COLOR_SOURCE_RGB;
        }
    }
    if (!has_color_mode &&
        page->color_source == LIGHT_COLOR_SOURCE_DEFAULT) {
        if (page->capabilities.supports_color_temp &&
            !page->capabilities.supports_color) {
            page->color_source = LIGHT_COLOR_SOURCE_COLOR_TEMP;
        }
        else if (page->capabilities.supports_color && page->has_rgb_color) {
            page->color_source = LIGHT_COLOR_SOURCE_RGB;
        }
    }
    light_remember_brightness_display_color(page);
    if (!(page->mode_mask & (1U << page->active_mode))) {
        if (page->mode_mask & LIGHT_MODE_MASK_BRIGHTNESS) {
            page->active_mode = LIGHT_MODE_BRIGHTNESS;
        }
        else if (page->mode_mask & LIGHT_MODE_MASK_RGB) {
            page->active_mode = LIGHT_MODE_RGB;
        }
        else if (page->mode_mask & LIGHT_MODE_MASK_COLOR_TEMP) {
            page->active_mode = LIGHT_MODE_COLOR_TEMP;
        }
    }

    char effect[LIGHT_EFFECT_NAME_MAX] = {0};
    if (page->capabilities.supports_effects &&
        ha_entity_store_get_attr_string(context->store,
                                        page->entity_id,
                                        "effect",
                                        effect,
                                        sizeof(effect)) == ESP_OK) {
        for (size_t i = 0; i < page->effect_count; i++) {
            if (strcmp(effect, page->effects[i]) == 0) {
                page->selected_effect = i;
                break;
            }
        }
    }
}

static void light_page_set_state_text(light_more_info_page_t *page,
                                      const char *override)
{
    if (!page || !page->state_label) {
        return;
    }
    char text[32] = {0};
    if (override) {
        snprintf(text, sizeof(text), "%s", override);
    }
    else if (!page->available) {
        snprintf(text, sizeof(text), "Unavailable");
    }
    else if (page->state_unknown) {
        snprintf(text, sizeof(text), "Unknown");
    }
    else if (!page->on) {
        snprintf(text, sizeof(text), "Off");
    }
    else if (page->capabilities.supports_brightness) {
        snprintf(text, sizeof(text), "%u%%", page->brightness_percent);
    }
    else {
        snprintf(text, sizeof(text), "On");
    }
    lv_label_set_text(page->state_label, text);
}

static void light_call_service(light_more_info_page_t *page,
                               const char *service,
                               cJSON *service_data)
{
    if (!page || !page->available) {
        return;
    }
    esp_err_t err = app_ha_call_entity_service_data("light",
                                                    service,
                                                    page->entity_id,
                                                    service_data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "light.%s for %s failed to send: %s",
                 service,
                 page->entity_id,
                 esp_err_to_name(err));
    }
}

static void light_brightness_changed(uint8_t percent,
                                     light_control_phase_t phase,
                                     void *user_data)
{
    light_more_info_page_t *page = user_data;
    if (!page) {
        return;
    }
    if (phase == LIGHT_CONTROL_CANCEL) {
        light_page_set_state_text(page, NULL);
        return;
    }
    if (phase != LIGHT_CONTROL_COMMIT) {
        return;
    }
    page->brightness_percent = percent;
    page->on = percent > 0;
    if (page->effect_selector) {
        light_effect_selector_set_power(page->effect_selector, page->on);
    }
    if (percent > 0 && page->session) {
        page->session->last_brightness_percent = percent;
        page->session->last_brightness_initialized = true;
    }
    light_page_set_state_text(page, NULL);
    if (percent == 0) {
        light_call_service(page, "turn_off", NULL);
        return;
    }
    cJSON *data = cJSON_CreateObject();
    if (data) {
        cJSON_AddNumberToObject(data, "brightness_pct", percent);
        light_call_service(page, "turn_on", data);
        cJSON_Delete(data);
    }
}

static void light_color_temp_changed(uint16_t kelvin,
                                     light_control_phase_t phase,
                                     void *user_data)
{
    light_more_info_page_t *page = user_data;
    if (!page || phase != LIGHT_CONTROL_COMMIT) {
        return;
    }
    page->kelvin = kelvin;
    page->color_source = LIGHT_COLOR_SOURCE_COLOR_TEMP;
    light_remember_brightness_display_color(page);
    page->on = true;
    if (page->effect_selector) {
        light_effect_selector_set_power(page->effect_selector, true);
    }
    cJSON *data = cJSON_CreateObject();
    if (data) {
        cJSON_AddNumberToObject(data, "color_temp_kelvin", kelvin);
        light_call_service(page, "turn_on", data);
        cJSON_Delete(data);
    }
}

static void light_rgb_changed(light_rgb_t rgb,
                              light_control_phase_t phase,
                              void *user_data)
{
    light_more_info_page_t *page = user_data;
    if (!page || phase != LIGHT_CONTROL_COMMIT) {
        return;
    }
    page->rgb = rgb;
    page->has_rgb_color = true;
    page->color_source = LIGHT_COLOR_SOURCE_RGB;
    light_remember_brightness_display_color(page);
    page->on = true;
    if (page->effect_selector) {
        light_effect_selector_set_power(page->effect_selector, true);
    }
    cJSON *data = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    if (data && array) {
        cJSON *red = cJSON_CreateNumber(rgb.red);
        cJSON *green = cJSON_CreateNumber(rgb.green);
        cJSON *blue = cJSON_CreateNumber(rgb.blue);
        if (red && green && blue) {
            cJSON_AddItemToArray(array, red);
            cJSON_AddItemToArray(array, green);
            cJSON_AddItemToArray(array, blue);
            red = NULL;
            green = NULL;
            blue = NULL;
            cJSON_AddItemToObject(data, "rgb_color", array);
            array = NULL;
            light_call_service(page, "turn_on", data);
        }
        cJSON_Delete(red);
        cJSON_Delete(green);
        cJSON_Delete(blue);
    }
    cJSON_Delete(array);
    cJSON_Delete(data);
}

static void light_send_preset_color(const char *entity_id,
                                    const light_favorite_color_t *color)
{
    if (!entity_id || !entity_id[0] || !color) {
        return;
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return;
    }
    const char *key = NULL;
    double values[5] = {0};
    size_t value_count = 0;
    if (color->type == LIGHT_FAVORITE_COLOR_TEMP) {
        cJSON_AddNumberToObject(data,
                                "color_temp_kelvin",
                                color->value.color_temp_kelvin);
    }
    else if (color->type == LIGHT_FAVORITE_RGB) {
        key = "rgb_color";
        values[0] = color->value.rgb.red;
        values[1] = color->value.rgb.green;
        values[2] = color->value.rgb.blue;
        value_count = 3;
    }
    else if (color->type == LIGHT_FAVORITE_HS) {
        key = "hs_color";
        values[0] = color->value.hs.hue;
        values[1] = color->value.hs.saturation;
        value_count = 2;
    }
    else if (color->type == LIGHT_FAVORITE_RGBW) {
        key = "rgbw_color";
        values[0] = color->value.rgbw.red;
        values[1] = color->value.rgbw.green;
        values[2] = color->value.rgbw.blue;
        values[3] = color->value.rgbw.white;
        value_count = 4;
    }
    else if (color->type == LIGHT_FAVORITE_RGBWW) {
        key = "rgbww_color";
        values[0] = color->value.rgbww.red;
        values[1] = color->value.rgbww.green;
        values[2] = color->value.rgbww.blue;
        values[3] = color->value.rgbww.cold_white;
        values[4] = color->value.rgbww.warm_white;
        value_count = 5;
    }

    if (key) {
        cJSON *array = cJSON_CreateDoubleArray(values, (int)value_count);
        if (!array || !cJSON_AddItemToObject(data, key, array)) {
            cJSON_Delete(array);
            cJSON_Delete(data);
            return;
        }
    }

    esp_err_t err = app_ha_call_entity_service_data("light",
                                                    "turn_on",
                                                    entity_id,
                                                    data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "light.turn_on for %s failed to send: %s",
                 entity_id,
                 esp_err_to_name(err));
    }
    cJSON_Delete(data);
}

static void light_preset_timer_cb(lv_timer_t *timer)
{
    light_more_info_session_t *session = lv_timer_get_user_data(timer);
    lv_timer_pause(timer);
    if (!session || !session->has_pending_preset) {
        return;
    }
    const light_favorite_color_t color = session->pending_preset;
    session->has_pending_preset = false;
    light_send_preset_color(session->preset_entity_id, &color);
}

static void light_schedule_preset(light_more_info_page_t *page,
                                  const light_favorite_color_t *color)
{
    if (!page || !page->available || !page->session || !color) {
        return;
    }
    light_more_info_session_t *session = page->session;
    session->pending_preset = *color;
    session->has_pending_preset = true;
    snprintf(session->preset_entity_id,
             sizeof(session->preset_entity_id),
             "%s",
             page->entity_id);

    if (!session->preset_timer) {
        session->preset_timer = lv_timer_create(light_preset_timer_cb,
                                                LIGHT_PRESET_DEBOUNCE_MS,
                                                session);
        if (!session->preset_timer) {
            session->has_pending_preset = false;
            light_send_preset_color(session->preset_entity_id, color);
        }
        return;
    }
    lv_timer_resume(session->preset_timer);
    lv_timer_reset(session->preset_timer);
}

static void light_preset_selected(size_t index,
                                  const light_favorite_color_t *color,
                                  void *user_data)
{
    (void)index;
    light_more_info_page_t *page = user_data;
    if (!page || !color) {
        return;
    }

    if (color->type == LIGHT_FAVORITE_COLOR_TEMP) {
        page->kelvin = color->value.color_temp_kelvin;
        page->color_source = LIGHT_COLOR_SOURCE_COLOR_TEMP;
    }
    else {
        page->rgb = color->display_rgb;
        page->has_rgb_color = true;
        page->color_source = LIGHT_COLOR_SOURCE_RGB;
    }
    light_remember_brightness_display_color(page);
    page->on = true;
    if (page->effect_selector) {
        light_effect_selector_set_power(page->effect_selector, true);
    }
    light_schedule_preset(page, color);
}

static void light_effect_changed(size_t index,
                                 const char *effect,
                                 void *user_data)
{
    light_more_info_page_t *page = user_data;
    if (!page || !effect) {
        return;
    }
    page->selected_effect = index;
    page->on = true;
    page->state_unknown = false;
    if (page->power_toggle) {
        light_power_toggle_set_value(page->power_toggle, true, LV_ANIM_ON);
    }
    if (page->assumed_power_buttons) {
        light_assumed_power_buttons_set_state(page->assumed_power_buttons,
                                              true,
                                              true);
    }
    if (page->effect_selector) {
        light_effect_selector_set_power(page->effect_selector, true);
    }
    cJSON *data = cJSON_CreateObject();
    if (data) {
        cJSON_AddStringToObject(data, "effect", effect);
        light_call_service(page, "turn_on", data);
        cJSON_Delete(data);
    }
}

static void light_power_toggle_changed(bool on, void *user_data)
{
    light_more_info_page_t *page = user_data;
    if (!page) {
        return;
    }
    page->on = on;
    page->state_unknown = false;
    light_page_set_state_text(page, NULL);
    if (page->effect_selector) {
        light_effect_selector_set_power(page->effect_selector, on);
    }
    light_call_service(page, on ? "turn_on" : "turn_off", NULL);
}

static void light_page_build_active_control(light_more_info_page_t *page)
{
    if (!page || !page->control_host) {
        return;
    }
    page->power_toggle = NULL;
    page->assumed_power_buttons = NULL;
    lv_obj_clean(page->control_host);
    lv_obj_t *control = NULL;
    if (page->mode_mask == 0) {
        if (page->assumed_state || page->state_unknown) {
            control = light_assumed_power_buttons_create(page->control_host);
            if (control) {
                page->assumed_power_buttons = control;
                light_assumed_power_buttons_set_state(
                    control,
                    !page->state_unknown,
                    page->on);
                light_assumed_power_buttons_set_disabled(control,
                                                         !page->available);
                light_assumed_power_buttons_set_change_cb(
                    control,
                    light_power_toggle_changed,
                    page);
            }
        }
        else {
            control = light_power_toggle_create(page->control_host);
            if (control) {
                page->power_toggle = control;
                light_power_toggle_set_value(control, page->on, LV_ANIM_OFF);
                light_power_toggle_set_disabled(control, !page->available);
                light_power_toggle_set_change_cb(control,
                                                 light_power_toggle_changed,
                                                 page);
            }
        }
    }
    else if (page->active_mode == LIGHT_MODE_RGB) {
        control = light_rgb_picker_create(page->control_host);
        if (control) {
            light_rgb_picker_set_value(control, page->rgb);
            light_rgb_picker_set_marker_visible(control, page->on);
            light_rgb_picker_set_disabled(control, !page->available);
            light_rgb_picker_set_change_cb(control, light_rgb_changed, page);
        }
    }
    else if (page->active_mode == LIGHT_MODE_COLOR_TEMP) {
        control = light_color_temp_slider_create(page->control_host);
        if (control) {
            light_color_temp_slider_set_range(control,
                                              page->min_kelvin,
                                              page->max_kelvin);
            light_color_temp_slider_set_value(control,
                                              page->kelvin,
                                              LV_ANIM_OFF);
            light_color_temp_slider_set_marker_visible(control, page->on);
            light_color_temp_slider_set_disabled(control, !page->available);
            light_color_temp_slider_set_change_cb(control,
                                                  light_color_temp_changed,
                                                  page);
        }
    }
    else {
        control = light_brightness_slider_create(page->control_host);
        if (control) {
            light_brightness_slider_set_value(control,
                                              page->brightness_percent,
                                              LV_ANIM_OFF);
            light_brightness_slider_set_active_color(
                control,
                light_brightness_display_color(page));
            light_brightness_slider_set_power(control, page->on);
            light_brightness_slider_set_disabled(control, !page->available);
            light_brightness_slider_set_change_cb(control,
                                                  light_brightness_changed,
                                                  page);
        }
    }
    if (control) {
        lv_obj_center(control);
    }
}

static void light_mode_bar_event(const light_mode_bar_event_t *event,
                                 void *user_data)
{
    light_more_info_page_t *page = user_data;
    if (!page || !event || !page->available) {
        return;
    }
    if (event->type == LIGHT_MODE_BAR_EVENT_POWER) {
        const bool turning_on = !page->on;
        light_call_service(page, turning_on ? "turn_on" : "turn_off", NULL);
        page->on = turning_on;
        light_mode_bar_set_power(page->mode_bar, page->on);
        if (page->effect_selector) {
            light_effect_selector_set_power(page->effect_selector, page->on);
        }
        light_page_set_state_text(page, turning_on ? "On" : NULL);
        return;
    }
    if (!(page->mode_mask & (1U << event->mode))) {
        return;
    }
    page->active_mode = event->mode;
    if (page->session) {
        page->session->active_mode = page->active_mode;
        page->session->active_mode_initialized = true;
    }
    light_mode_bar_set_active_mode(page->mode_bar, page->active_mode);
    light_page_build_active_control(page);
}

static void light_page_delete_cb(lv_event_t *event)
{
    lv_free(lv_event_get_user_data(event));
}

static void light_more_info_close_event_cb(lv_event_t *event)
{
    light_more_info_page_t *page = lv_event_get_user_data(event);
    if (page && page->close) {
        page->close(page->close_user_data);
    }
}

static lv_obj_t *light_create_clear_container(lv_obj_t *parent,
                                              int32_t x,
                                              int32_t y,
                                              int32_t width,
                                              int32_t height)
{
    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) {
        return NULL;
    }
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    return obj;
}

static void light_control_overflow_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(event, LIGHT_CONTROL_OVERFLOW_SIZE);
    }
}

static void light_enable_control_overflow(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(obj,
                        light_control_overflow_event_cb,
                        LV_EVENT_REFR_EXT_DRAW_SIZE,
                        NULL);
    lv_obj_refresh_ext_draw_size(obj);
}

static void light_build_header(lv_obj_t *root,
                               lv_obj_t *overlay,
                               const entity_more_info_context_t *context,
                               light_more_info_page_t *page)
{
    lv_obj_t *header = lv_obj_create(root);
    lv_obj_remove_style_all(header);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 20, 0);
    lv_obj_set_style_pad_ver(header, 12, 0);
    lv_obj_set_style_pad_gap(header, 10, 0);

    lv_obj_t *close_button = lv_image_create(header);
    lv_img_set_src(close_button, &close_50);
    lv_obj_add_flag(close_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_button,
                        light_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        page);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_height(title,
                      lv_font_get_line_height(&ha_font_misan_medium_24));

    lv_obj_t *close_hitbox = light_create_clear_container(overlay,
                                                          20,
                                                          12,
                                                          50,
                                                          50);
    lv_obj_add_flag(close_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_hitbox,
                        light_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        page);
}

static void light_build_state_block(lv_obj_t *parent,
                                    const entity_more_info_context_t *context,
                                    light_more_info_page_t *page)
{
    lv_obj_t *state_block = light_create_clear_container(
        parent,
        0,
        0,
        LV_PCT(100),
        LIGHT_STATE_BLOCK_HEIGHT);
    if (!state_block) {
        return;
    }
    lv_obj_set_layout(state_block, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(state_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(state_block,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(state_block, 0, 0);

    page->state_label = lv_label_create(state_block);
    lv_obj_set_size(page->state_label, 280, 28);
    lv_obj_set_style_text_font(page->state_label, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(page->state_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(page->state_label, LV_TEXT_ALIGN_CENTER, 0);
    light_page_set_state_text(page, NULL);

    char time_text[48] = {0};
    if (context->snapshot.last_changed[0] &&
        ha_datetime_format_relative_now(context->snapshot.last_changed,
                                        time_text,
                                        sizeof(time_text)) != ESP_OK) {
        time_text[0] = '\0';
    }
    lv_obj_t *time_label = lv_label_create(state_block);
    lv_obj_set_size(time_label, 280, 24);
    lv_label_set_text(time_label, time_text);
    lv_obj_set_style_text_font(time_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
    time_t changed_time = 0;
    if (context->snapshot.last_changed[0] &&
        ha_datetime_parse_epoch(context->snapshot.last_changed,
                                &changed_time) == ESP_OK) {
        ha_relative_time_label_bind(time_label, changed_time);
    }
}

static void light_build_control_page(lv_obj_t *parent,
                                     light_more_info_page_t *page)
{
    light_enable_control_overflow(parent);
    page->control_host = parent;
    light_page_build_active_control(page);
}

static void light_build_mode_bar(lv_obj_t *parent,
                                 light_more_info_page_t *page)
{
    if (!page || page->mode_mask == 0) {
        return;
    }
    page->mode_bar = light_mode_bar_create(parent);
    if (page->mode_bar) {
        light_mode_bar_set_modes(page->mode_bar, page->mode_mask);
        light_mode_bar_set_active_mode(page->mode_bar, page->active_mode);
        light_mode_bar_set_power(page->mode_bar, page->on);
        light_mode_bar_set_disabled(page->mode_bar, !page->available);
        light_mode_bar_set_event_cb(page->mode_bar,
                                    light_mode_bar_event,
                                    page);
    }
}

static void light_build_first_page(lv_obj_t *parent,
                                   light_more_info_page_t *page)
{
    lv_obj_t *page_content = light_create_clear_container(parent,
                                                           0,
                                                           0,
                                                           LV_PCT(100),
                                                           LV_PCT(100));
    if (!page_content) {
        return;
    }
    lv_obj_set_layout(page_content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(page_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page_content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(page_content, 0, 0);
    lv_obj_set_style_pad_gap(page_content, LIGHT_CONTENT_GAP, 0);

    lv_obj_t *control = light_create_clear_container(
        page_content,
        0,
        0,
        320,
        LIGHT_CONTROL_AREA_HEIGHT);
    if (control) {
        light_build_control_page(control, page);
    }
    light_build_mode_bar(page_content, page);
}

static void light_update_dots(light_more_info_page_t *page)
{
    if (!page || !page->tileview) {
        return;
    }

    lv_obj_t *active = lv_tileview_get_tile_active(page->tileview);
    for (size_t i = 0; i < 2; i++) {
        if (!page->page_dots[i]) {
            continue;
        }
        const bool selected = active == page->tileview_pages[i];
        lv_obj_set_style_bg_color(page->page_dots[i],
                                  lv_color_hex(selected ? 0xFBFBFB
                                                        : 0x999999),
                                  0);
        lv_obj_set_style_bg_opa(page->page_dots[i],
                                selected ? LV_OPA_COVER : LV_OPA_40,
                                0);
        if (selected && page->session) {
            page->session->tileview_page = (uint8_t)i;
        }
    }
}

static light_rgb_t light_kelvin_rgb(uint16_t kelvin)
{
    const float temperature = kelvin / 100.0f;
    const float red = temperature <= 66.0f
                          ? 255.0f
                          : 329.698727446f *
                                powf(temperature - 60.0f, -0.1332047592f);
    const float green = temperature <= 66.0f
                            ? 99.4708025861f * logf(temperature) -
                                  161.1195681661f
                            : 288.1221695283f *
                                  powf(temperature - 60.0f,
                                       -0.0755148492f);
    const float blue = temperature >= 66.0f
                           ? 255.0f
                           : temperature <= 19.0f
                                 ? 0.0f
                                 : 138.5177312231f *
                                           logf(temperature - 10.0f) -
                                       305.0447927307f;
    return (light_rgb_t) {
        .red = (uint8_t)LV_CLAMP(0, (int32_t)lroundf(red), 255),
        .green = (uint8_t)LV_CLAMP(0, (int32_t)lroundf(green), 255),
        .blue = (uint8_t)LV_CLAMP(0, (int32_t)lroundf(blue), 255),
    };
}

static lv_color_t light_kelvin_color(uint16_t kelvin)
{
    const light_rgb_t rgb = light_kelvin_rgb(kelvin);
    return lv_color_make(rgb.red, rgb.green, rgb.blue);
}

static uint8_t light_scale_channel(double value)
{
    return (uint8_t)LV_CLAMP(0, (int32_t)lround(value), 255);
}

static light_rgb_t light_hs_rgb(light_hs_t hs)
{
    double hue = fmod(hs.hue, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    const double saturation = LV_CLAMP(0.0, hs.saturation / 100.0, 1.0);
    const double chroma = 255.0 * saturation;
    const double section = hue / 60.0;
    const double x = chroma * (1.0 - fabs(fmod(section, 2.0) - 1.0));
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    if (section < 1.0) {
        red = chroma;
        green = x;
    }
    else if (section < 2.0) {
        red = x;
        green = chroma;
    }
    else if (section < 3.0) {
        green = chroma;
        blue = x;
    }
    else if (section < 4.0) {
        green = x;
        blue = chroma;
    }
    else if (section < 5.0) {
        red = x;
        blue = chroma;
    }
    else {
        red = chroma;
        blue = x;
    }
    const double match = 255.0 - chroma;
    return (light_rgb_t) {
        light_scale_channel(red + match),
        light_scale_channel(green + match),
        light_scale_channel(blue + match),
    };
}

static light_rgb_t light_match_max_rgb(double red,
                                       double green,
                                       double blue,
                                       uint8_t input_max)
{
    const double output_max = LV_MAX(red, LV_MAX(green, blue));
    const double scale = output_max > 0.0 ? input_max / output_max : 0.0;
    return (light_rgb_t) {
        light_scale_channel(red * scale),
        light_scale_channel(green * scale),
        light_scale_channel(blue * scale),
    };
}

static light_rgb_t light_rgbw_rgb(light_rgbw_t rgbw)
{
    const uint8_t input_max = LV_MAX(
        LV_MAX(rgbw.red, rgbw.green),
        LV_MAX(rgbw.blue, rgbw.white));
    return light_match_max_rgb(rgbw.red + rgbw.white,
                               rgbw.green + rgbw.white,
                               rgbw.blue + rgbw.white,
                               input_max);
}

static light_rgb_t light_rgbww_rgb(light_rgbww_t rgbww,
                                   uint16_t min_kelvin,
                                   uint16_t max_kelvin)
{
    const uint8_t input_max = LV_MAX(
        LV_MAX(LV_MAX(rgbww.red, rgbww.green), rgbww.blue),
        LV_MAX(rgbww.cold_white, rgbww.warm_white));
    const uint16_t safe_min = LV_MAX((uint16_t)1, min_kelvin);
    const uint16_t safe_max = LV_MAX(safe_min, max_kelvin);
    const uint32_t max_mired = 1000000U / safe_min;
    const uint32_t min_mired = 1000000U / safe_max;
    const uint16_t white_sum = rgbww.cold_white + rgbww.warm_white;
    const double warm_ratio = white_sum > 0
                                  ? (double)rgbww.warm_white / white_sum
                                  : 0.5;
    const double color_mired =
        min_mired + warm_ratio * (max_mired - min_mired);
    const uint16_t kelvin = color_mired > 0.0
                                ? (uint16_t)(1000000.0 / color_mired)
                                : safe_max;
    const light_rgb_t white = light_kelvin_rgb(kelvin);
    const double white_level =
        LV_MAX(rgbww.cold_white, rgbww.warm_white) / 255.0;
    return light_match_max_rgb(rgbww.red + white.red * white_level,
                               rgbww.green + white.green * white_level,
                               rgbww.blue + white.blue * white_level,
                               input_max);
}

static void light_prepare_favorite_display_colors(light_more_info_page_t *page)
{
    if (!page) {
        return;
    }
    for (size_t i = 0; i < page->favorite_color_count; i++) {
        light_favorite_color_t *color = &page->favorite_colors[i];
        switch (color->type) {
        case LIGHT_FAVORITE_COLOR_TEMP:
            color->display_rgb =
                light_kelvin_rgb(color->value.color_temp_kelvin);
            break;
        case LIGHT_FAVORITE_RGB:
            color->display_rgb = color->value.rgb;
            break;
        case LIGHT_FAVORITE_HS:
            color->display_rgb = light_hs_rgb(color->value.hs);
            break;
        case LIGHT_FAVORITE_RGBW:
            color->display_rgb = light_rgbw_rgb(color->value.rgbw);
            break;
        case LIGHT_FAVORITE_RGBWW:
            color->display_rgb = light_rgbww_rgb(color->value.rgbww,
                                                 page->min_kelvin,
                                                 page->max_kelvin);
            break;
        }
    }
}

static size_t light_build_default_favorite_colors(
    const light_more_info_page_t *page,
    light_favorite_color_t colors[LIGHT_FAVORITE_COLOR_MAX])
{
    if (!page || !colors) {
        return 0;
    }

    size_t count = 0;
    if (page->capabilities.supports_color_temp) {
        const uint16_t min_kelvin = page->min_kelvin;
        const uint16_t max_kelvin = LV_MAX(page->min_kelvin,
                                          page->max_kelvin);
        const uint32_t span = max_kelvin - min_kelvin;
        for (size_t i = 0; i < LIGHT_COLOR_TEMP_FAVORITE_COUNT; i++) {
            const uint16_t kelvin = (uint16_t)(
                min_kelvin +
                (span * i + (LIGHT_COLOR_TEMP_FAVORITE_COUNT - 1) / 2) /
                    (LIGHT_COLOR_TEMP_FAVORITE_COUNT - 1));
            colors[count++] = (light_favorite_color_t) {
                .type = LIGHT_FAVORITE_COLOR_TEMP,
                .display_rgb = light_kelvin_rgb(kelvin),
                .value.color_temp_kelvin = kelvin,
            };
        }
    }
    else if (page->capabilities.supports_color) {
        static const uint16_t white_kelvin[] = {2000, 3500, 5000, 6500};
        for (size_t i = 0; i < sizeof(white_kelvin) / sizeof(white_kelvin[0]);
             i++) {
            const light_rgb_t rgb = light_kelvin_rgb(white_kelvin[i]);
            colors[count++] = (light_favorite_color_t) {
                .type = LIGHT_FAVORITE_RGB,
                .display_rgb = rgb,
                .value.rgb = rgb,
            };
        }
    }

    if (page->capabilities.supports_color) {
        static const light_rgb_t colored[] = {
            {127, 172, 255},
            {215, 150, 255},
            {255, 158, 243},
            {255, 110, 84},
        };
        for (size_t i = 0; i < sizeof(colored) / sizeof(colored[0]); i++) {
            colors[count++] = (light_favorite_color_t) {
                .type = LIGHT_FAVORITE_RGB,
                .display_rgb = colored[i],
                .value.rgb = colored[i],
            };
        }
    }
    return count;
}

static lv_color_t light_brightness_display_color(
    const light_more_info_page_t *page)
{
    if (page && page->color_source == LIGHT_COLOR_SOURCE_COLOR_TEMP) {
        return light_kelvin_color(page->kelvin);
    }
    if (page && page->color_source == LIGHT_COLOR_SOURCE_RGB &&
        page->has_rgb_color) {
        return lv_color_make(page->rgb.red,
                             page->rgb.green,
                             page->rgb.blue);
    }
    if (page && page->session &&
        page->session->last_brightness_display_color_initialized) {
        return page->session->last_brightness_display_color;
    }
    return lv_color_hex(0xFFC107);
}

static void light_remember_brightness_display_color(
    light_more_info_page_t *page)
{
    if (!page || !page->session) {
        return;
    }
    if (page->color_source == LIGHT_COLOR_SOURCE_COLOR_TEMP) {
        page->session->last_brightness_display_color =
            light_kelvin_color(page->kelvin);
    }
    else if (page->color_source == LIGHT_COLOR_SOURCE_RGB &&
             page->has_rgb_color) {
        page->session->last_brightness_display_color =
            lv_color_make(page->rgb.red,
                          page->rgb.green,
                          page->rgb.blue);
    }
    else {
        return;
    }
    page->session->last_brightness_display_color_initialized = true;
}

static bool light_tile_read_rgb_color(ha_entity_store_t *store,
                                      const char *entity_id,
                                      lv_color_t *color)
{
    if (!store || !entity_id || !color) {
        return false;
    }

    cJSON *rgb = NULL;
    if (ha_entity_store_dup_attr_json(store,
                                      entity_id,
                                      "rgb_color",
                                      &rgb) != ESP_OK) {
        return false;
    }

    bool valid = false;
    if (cJSON_IsArray(rgb) && cJSON_GetArraySize(rgb) >= 3) {
        const cJSON *red = cJSON_GetArrayItem(rgb, 0);
        const cJSON *green = cJSON_GetArrayItem(rgb, 1);
        const cJSON *blue = cJSON_GetArrayItem(rgb, 2);
        if (cJSON_IsNumber(red) && cJSON_IsNumber(green) &&
            cJSON_IsNumber(blue)) {
            *color = lv_color_make(
                (uint8_t)LV_CLAMP(0, (int32_t)lround(red->valuedouble), 255),
                (uint8_t)LV_CLAMP(0,
                                  (int32_t)lround(green->valuedouble),
                                  255),
                (uint8_t)LV_CLAMP(0,
                                  (int32_t)lround(blue->valuedouble),
                                  255));
            valid = true;
        }
    }
    cJSON_Delete(rgb);
    return valid;
}

static lv_color_t light_tile_color_temp_color(ha_entity_store_t *store,
                                               const char *entity_id)
{
    double value = 0;
    int32_t minimum = 2000;
    int32_t maximum = 6535;
    int32_t kelvin = 4000;
    if (light_get_number(store,
                         entity_id,
                         "min_color_temp_kelvin",
                         &value)) {
        minimum = LV_CLAMP(1, (int32_t)lround(value), 65535);
    }
    if (light_get_number(store,
                         entity_id,
                         "max_color_temp_kelvin",
                         &value)) {
        maximum = LV_CLAMP(minimum, (int32_t)lround(value), 65535);
    }
    if (light_get_number(store, entity_id, "color_temp_kelvin", &value)) {
        kelvin = LV_CLAMP(minimum, (int32_t)lround(value), maximum);
    }
    else {
        kelvin = LV_CLAMP(minimum, kelvin, maximum);
    }
    return light_kelvin_color((uint16_t)kelvin);
}

static lv_color_t light_tile_active_color(ha_entity_store_t *store,
                                          const char *entity_id)
{
    const lv_color_t default_color = lv_color_hex(0xFFC107);
    char color_mode[24] = {0};
    const bool has_color_mode =
        ha_entity_store_get_attr_string(store,
                                        entity_id,
                                        "color_mode",
                                        color_mode,
                                        sizeof(color_mode)) == ESP_OK &&
        color_mode[0] != '\0';
    if (has_color_mode) {
        if (strcmp(color_mode, "color_temp") == 0) {
            return light_tile_color_temp_color(store, entity_id);
        }
        if (light_color_mode_is_rgb(color_mode)) {
            lv_color_t rgb_color;
            return light_tile_read_rgb_color(store, entity_id, &rgb_color)
                       ? rgb_color
                       : default_color;
        }
        return default_color;
    }

    light_capabilities_t capabilities = {0};
    cJSON *supported_modes = NULL;
    if (ha_entity_store_dup_attr_json(store,
                                      entity_id,
                                      "supported_color_modes",
                                      &supported_modes) == ESP_OK) {
        light_analyze_supported_modes(supported_modes, &capabilities);
        cJSON_Delete(supported_modes);
    }
    if (capabilities.supports_color_temp && !capabilities.supports_color) {
        return light_tile_color_temp_color(store, entity_id);
    }
    if (capabilities.supports_color) {
        lv_color_t rgb_color;
        if (light_tile_read_rgb_color(store, entity_id, &rgb_color)) {
            return rgb_color;
        }
    }
    return default_color;
}

static lv_color_t light_tile_icon_circle_color(lv_color_t active_color)
{
    /* Preserve the existing dark-circle treatment while tinting it with the
     * current light color. Yellow 0xFFC107 resolves to approximately the
     * previous 0x493D17 circle color. */
    return lv_color_mix(active_color,
                        lv_color_hex(0x1C1C1C),
                        LV_OPA_20);
}

static void light_tileview_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    light_update_dots(lv_event_get_user_data(event));
}

static void light_build_dots(lv_obj_t *parent,
                             light_more_info_page_t *page)
{
    lv_obj_t *dots_host = light_create_clear_container(parent,
                                                        0,
                                                        0,
                                                        LV_SIZE_CONTENT,
                                                        LV_SIZE_CONTENT);
    if (!dots_host) {
        return;
    }
    lv_obj_set_layout(dots_host, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(dots_host, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_host,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dots_host, 0, 0);
    lv_obj_set_style_pad_gap(dots_host, 8, 0);

    for (size_t i = 0; i < 2; i++) {
        page->page_dots[i] = lv_obj_create(dots_host);
        lv_obj_remove_style_all(page->page_dots[i]);
        lv_obj_remove_flag(page->page_dots[i],
                           LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(page->page_dots[i], 8, 8);
        lv_obj_set_style_radius(page->page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(page->page_dots[i], LV_OPA_COVER, 0);
    }

    light_update_dots(page);
}

static void light_build_second_page(lv_obj_t *parent,
                                    light_more_info_page_t *page)
{
    lv_obj_t *container = light_create_clear_container(parent,
                                                        0,
                                                        0,
                                                        LV_PCT(100),
                                                        LV_SIZE_CONTENT);
    if (!container) {
        return;
    }
    lv_obj_align(container,
                 LV_ALIGN_TOP_MID,
                 0,
                 LIGHT_SECOND_PAGE_TOP_OFFSET);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_pad_gap(container, LIGHT_SECOND_PAGE_GAP, 0);

    if (page->capabilities.supports_favorite_colors) {
        lv_obj_t *presets = light_preset_colors_create(container);
        if (presets) {
            light_favorite_color_t colors[LIGHT_FAVORITE_COLOR_MAX];
            size_t color_count = page->favorite_color_count;
            if (page->has_custom_favorite_colors) {
                memcpy(colors,
                       page->favorite_colors,
                       color_count * sizeof(colors[0]));
            }
            else {
                color_count =
                    light_build_default_favorite_colors(page, colors);
            }
            light_preset_colors_set_palette(presets, colors, color_count);
            light_preset_colors_set_disabled(presets, !page->available);
            light_preset_colors_set_select_cb(presets,
                                              light_preset_selected,
                                              page);
        }
    }

    if (page->capabilities.supports_effects) {
        const char *options[LIGHT_EFFECT_MAX_COUNT] = {0};
        for (size_t i = 0; i < page->effect_count; i++) {
            options[i] = page->effects[i];
        }
        lv_obj_t *effect = light_effect_selector_create(container);
        if (effect) {
            page->effect_selector = effect;
            light_effect_selector_set_options(effect,
                                              options,
                                              page->effect_count);
            light_effect_selector_set_selected(effect,
                                               page->selected_effect);
            light_effect_selector_set_power(effect, page->on);
            light_effect_selector_set_disabled(effect, !page->available);
            light_effect_selector_set_change_cb(effect,
                                                light_effect_changed,
                                                page);
        }
    }
}

static esp_err_t light_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->store || !context->close) {
        return ESP_ERR_INVALID_ARG;
    }

    light_more_info_page_t *page = lv_calloc(1, sizeof(*page));
    if (!page) {
        return ESP_ERR_NO_MEM;
    }
    page->session = light_get_session(context->screen);
    if (!page->session) {
        lv_free(page);
        return ESP_ERR_NO_MEM;
    }
    light_read_page_state(context, page);
    if (page->session->active_mode_initialized &&
        (page->mode_mask & (1U << page->session->active_mode))) {
        page->active_mode = page->session->active_mode;
    }
    else {
        /* A newly opened More Info page starts from the first control mode.
         * Keep later user selections in the session so HA refreshes do not
         * force the page back to brightness while it remains open. */
        if (page->mode_mask & LIGHT_MODE_MASK_BRIGHTNESS) {
            page->active_mode = LIGHT_MODE_BRIGHTNESS;
        }
        page->session->active_mode = page->active_mode;
        page->session->active_mode_initialized = true;
    }

    lv_obj_t *screen = context->screen;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x010203), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_t *root = light_create_clear_container(screen, 0, 0, 320, 480);
    if (!root) {
        lv_free(page);
        return ESP_ERR_NO_MEM;
    }
    lv_obj_add_event_cb(root, light_page_delete_cb, LV_EVENT_DELETE, page);
    /* Container S: the complete More Info content. */
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_pad_gap(root, 0, 0);

    /* Container A: header. */
    light_build_header(root, context->overlay, context, page);

    /* Container B: state, controls, mode bar and page dots. */
    lv_obj_t *content = light_create_clear_container(root,
                                                      0,
                                                      0,
                                                      LV_PCT(100),
                                                      0);
    if (!content) {
        lv_obj_delete(root);
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_gap(content, LIGHT_CONTENT_GAP, 0);

    light_build_state_block(content, context, page);

    bool has_second_page =
        page->capabilities.supports_favorite_colors ||
        page->capabilities.supports_effects;
    if (has_second_page) {
        page->tileview = lv_tileview_create(content);
        lv_obj_set_size(page->tileview, 320, LIGHT_PAGE_AREA_HEIGHT);
        lv_obj_set_style_bg_opa(page->tileview, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(page->tileview, 0, 0);
        lv_obj_set_style_pad_all(page->tileview, 0, 0);
        lv_obj_set_scrollbar_mode(page->tileview, LV_SCROLLBAR_MODE_OFF);
        light_enable_control_overflow(page->tileview);
        page->tileview_pages[0] = lv_tileview_add_tile(page->tileview,
                                                       0,
                                                       0,
                                                       LV_DIR_RIGHT);
        page->tileview_pages[1] = lv_tileview_add_tile(page->tileview,
                                                       1,
                                                       0,
                                                       LV_DIR_LEFT);
        for (size_t i = 0; i < 2; i++) {
            lv_obj_set_style_bg_opa(page->tileview_pages[i],
                                    LV_OPA_TRANSP,
                                    0);
            lv_obj_set_style_border_width(page->tileview_pages[i], 0, 0);
            lv_obj_set_style_pad_all(page->tileview_pages[i], 0, 0);
        }
        light_build_first_page(page->tileview_pages[0], page);
        light_build_second_page(page->tileview_pages[1], page);
        lv_obj_add_event_cb(page->tileview,
                            light_tileview_event_cb,
                            LV_EVENT_VALUE_CHANGED,
                            page);
        lv_tileview_set_tile_by_index(
            page->tileview,
            page->session->tileview_page == 1 ? 1 : 0,
            0,
            LV_ANIM_OFF);
    }
    else {
        page->session->tileview_page = 0;
        lv_obj_t *control = light_create_clear_container(
            content,
            0,
            0,
            320,
            LIGHT_CONTROL_AREA_HEIGHT);
        light_build_control_page(control, page);
        light_build_mode_bar(content, page);
    }
    if (has_second_page) {
        light_build_dots(content, page);
    }
    return ESP_OK;
}

static bool light_handle_action(const entity_action_context_t *context)
{
    if (entity_handler_handle_more_info_action(context)) {
        return true;
    }
    if (!context || !context->entity_id) {
        return false;
    }
    const screen_action_t *action = entity_handler_get_action(context);
    if (!action || action->action != SCREEN_ACTION_TOGGLE) {
        return false;
    }
    esp_err_t err = app_ha_call_entity_service("light",
                                               "toggle",
                                               context->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "light.toggle for %s failed to send: %s",
                 context->entity_id,
                 esp_err_to_name(err));
    }
    return true;
}

static esp_err_t light_render_tile(ha_entity_store_t *store,
                                   const ha_entity_snapshot_t *snapshot,
                                   entity_display_model_t *model)
{
    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = MDI_SYMBOL_LIGHTBULB_OFF;
    model->background_color = lv_color_hex(0x1E2228);
    model->icon_color = lv_color_hex(0x9e9e9e);
    model->icon_circle_background_color = lv_color_hex(0x363636);
    model->has_ripple_color = true;
    model->ripple_color = lv_color_hex(0xD9D9D9);
    model->title_color = lv_color_hex(0xFFFFFF);
    model->state_color = lv_color_hex(0x666666);

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon = MDI_SYMBOL_LIGHTBULB;
        return ESP_OK;
    }

    if (strcmp(snapshot->state, "on") == 0) {
        model->icon = MDI_SYMBOL_LIGHTBULB;
        model->icon_color = light_tile_active_color(store,
                                                    snapshot->entity_id);
        model->icon_circle_background_color =
            light_tile_icon_circle_color(model->icon_color);

        double brightness = 0;
        if (ha_entity_store_get_attr_number(store,
                                            snapshot->entity_id,
                                            "brightness",
                                            &brightness) == ESP_OK) {
            double percent = LV_CLAMP((double)LIGHT_MIN_BRIGHTNESS_PERCENT,
                                      brightness * 100.0 / 255.0,
                                      100.0);
            snprintf(model->state_text,
                     sizeof(model->state_text),
                     "Open - %.0f%%",
                     percent);
            return ESP_OK;
        }
    }

    entity_handler_copy_snapshot_state(model->state_text,
                                       sizeof(model->state_text),
                                       snapshot);
    return ESP_OK;
}

const entity_handler_t light_entity_handler = {
    .domain = "light",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_HOME,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_TOGGLE },
    },
    .handle_action = light_handle_action,
    .render_tile = light_render_tile,
    .render_more_info = light_render_more_info,
};
