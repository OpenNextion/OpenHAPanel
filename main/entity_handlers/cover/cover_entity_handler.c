#include "cover_entity_handler.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_log.h"
#include "screen/components/ha_relative_time_label.h"
#include "screen/components/more_info_screen.h"

static const char *TAG = "cover_entity_handler";

#define COVER_FEATURE_OPEN          (1U << 0)
#define COVER_FEATURE_CLOSE         (1U << 1)
#define COVER_FEATURE_SET_POSITION  (1U << 2)
#define COVER_FEATURE_STOP          (1U << 3)
#define COVER_FEATURE_OPEN_TILT      (1U << 4)
#define COVER_FEATURE_CLOSE_TILT     (1U << 5)
#define COVER_FEATURE_STOP_TILT      (1U << 6)
#define COVER_FEATURE_SET_TILT_POS   (1U << 7)

#define COVER_DETAIL_WIDTH          280
#define COVER_CONTROL_HEIGHT        230
#define COVER_BUTTON_WIDTH          82
#define COVER_BUTTON_HEIGHT         70
#define COVER_BUTTON_RADIUS         20
#define COVER_MODE_WIDTH            104
#define COVER_MODE_HEIGHT           48
#define COVER_PRESET_HEIGHT         40

#define COVER_BG                    0x010203
#define COVER_CONTROL_BG            0x1F1F1F
#define COVER_CONTROL_PRESSED       0x353535
#define COVER_DISABLED_BG           0x171717
#define COVER_DISABLED_FG           0x666666
#define COVER_POSITION_TRACK        0x1D142B
#define COVER_POSITION_ACTIVE       0x9065CC
#define COVER_POSITION_WIDTH        80
#define COVER_POSITION_HEIGHT       COVER_CONTROL_HEIGHT
#define COVER_POSITION_INNER_RADIUS 12
#define COVER_POSITION_KNOB_WIDTH   52
#define COVER_POSITION_KNOB_HEIGHT  4
#define COVER_POSITION_KNOB_MARGIN  10
#define COVER_POSITION_ANIMATION_MS 180
#define COVER_POSITION_MIN_VALUE    ((2 * COVER_POSITION_KNOB_MARGIN) + \
                                     COVER_POSITION_KNOB_HEIGHT)
#define COVER_POSITION_TRAVEL       (COVER_POSITION_HEIGHT - \
                                     COVER_POSITION_MIN_VALUE)
#define COVER_TOGGLE_TRACK_WIDTH     88
#define COVER_TOGGLE_TRACK_HEIGHT    230
#define COVER_TOGGLE_KNOB_WIDTH      76
#define COVER_TOGGLE_KNOB_HEIGHT     109
#define COVER_TOGGLE_INSET           6
#define COVER_TOGGLE_RADIUS          20
#define COVER_TOGGLE_OPEN            0x9065CC
#define COVER_TOGGLE_CLOSED          0xA6A6A6
#define COVER_TOGGLE_DISABLED_TRACK  0x242424
#define COVER_TOGGLE_DISABLED_KNOB   0x555555
#define COVER_TOGGLE_TRACK_OPA       51
#define COVER_TOGGLE_PRESSED_OPA     102
#define COVER_TOGGLE_ANIMATION_MS     180
#define COVER_TOGGLE_TOP_Y            COVER_TOGGLE_INSET
#define COVER_TOGGLE_BOTTOM_Y         (COVER_TOGGLE_TRACK_HEIGHT - \
                                       COVER_TOGGLE_KNOB_HEIGHT - \
                                       COVER_TOGGLE_INSET)

typedef enum {
    COVER_DETAIL_MODE_BUTTONS = 0,
    COVER_DETAIL_MODE_POSITION,
} cover_detail_mode_t;

typedef enum {
    COVER_ACTION_OPEN = 0,
    COVER_ACTION_STOP,
    COVER_ACTION_CLOSE,
} cover_action_t;

typedef struct {
    lv_obj_t *active_bar;
    lv_obj_t *knob;
    lv_obj_t *value_label;
    int32_t visual_value;
    int32_t last_drag_value;
    bool has_last_drag_value;
} cover_position_view_t;

static char s_cover_detail_entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
static cover_detail_mode_t s_cover_detail_mode = COVER_DETAIL_MODE_BUTTONS;
static lv_obj_t *s_cover_detail_screen;
static bool s_cover_toggle_visual_valid;
static bool s_cover_toggle_visual_on;
static bool s_cover_position_visual_valid;
static int32_t s_cover_position_visual_value;

static void cover_toggle_set_knob_y(void *object, int32_t y);
static void cover_toggle_animate_knob(lv_obj_t *knob, int32_t target_y);

/* Keep this mapping aligned with Home Assistant's cover entity-component icon
 * resources. HA uses the device-class default for open/unknown/unavailable and
 * only replaces it when the current state has an explicit icon entry. */
typedef struct {
    const char *device_class;
    const char *icon_default;
    const char *icon_closed;
    const char *icon_closing;
    const char *icon_opening;
} cover_tile_icons_t;

static const cover_tile_icons_t COVER_TILE_DEFAULT_ICONS = {
    .icon_default = MDI_SYMBOL_WINDOW_OPEN,
    .icon_closed = MDI_SYMBOL_WINDOW_CLOSED,
    .icon_closing = MDI_SYMBOL_ARROW_DOWN_BOX,
    .icon_opening = MDI_SYMBOL_ARROW_UP_BOX,
};

static const cover_tile_icons_t COVER_TILE_ICONS[] = {
    /* HA Core has no entity_component icon entry for awning. It therefore
     * intentionally falls through to COVER_TILE_DEFAULT_ICONS. */
    {
        .device_class = "blind",
        .icon_default = MDI_SYMBOL_BLINDS_HORIZONTAL,
        .icon_closed = MDI_SYMBOL_BLINDS_HORIZONTAL_CLOSED,
        .icon_closing = MDI_SYMBOL_ARROW_DOWN_BOX,
        .icon_opening = MDI_SYMBOL_ARROW_UP_BOX,
    },
    {
        .device_class = "curtain",
        .icon_default = MDI_SYMBOL_CURTAINS,
        .icon_closed = MDI_SYMBOL_CURTAINS_CLOSED,
        .icon_closing = MDI_SYMBOL_ARROW_COLLAPSE_HORIZONTAL,
        .icon_opening = MDI_SYMBOL_ARROW_SPLIT_VERTICAL,
    },
    {
        .device_class = "damper",
        .icon_default = MDI_SYMBOL_CIRCLE,
        .icon_closed = MDI_SYMBOL_CIRCLE_SLICE_8,
    },
    {
        .device_class = "door",
        .icon_default = MDI_SYMBOL_DOOR_OPEN,
        .icon_closed = MDI_SYMBOL_DOOR_CLOSED,
    },
    {
        .device_class = "garage",
        .icon_default = MDI_SYMBOL_GARAGE_OPEN,
        .icon_closed = MDI_SYMBOL_GARAGE,
        .icon_closing = MDI_SYMBOL_ARROW_DOWN_BOX,
        .icon_opening = MDI_SYMBOL_ARROW_UP_BOX,
    },
    {
        .device_class = "gate",
        .icon_default = MDI_SYMBOL_GATE_OPEN,
        .icon_closed = MDI_SYMBOL_GATE,
        .icon_closing = MDI_SYMBOL_ARROW_RIGHT,
        .icon_opening = MDI_SYMBOL_ARROW_RIGHT,
    },
    {
        .device_class = "shade",
        .icon_default = MDI_SYMBOL_ROLLER_SHADE,
        .icon_closed = MDI_SYMBOL_ROLLER_SHADE_CLOSED,
        .icon_closing = MDI_SYMBOL_ARROW_DOWN_BOX,
        .icon_opening = MDI_SYMBOL_ARROW_UP_BOX,
    },
    {
        .device_class = "shutter",
        .icon_default = MDI_SYMBOL_WINDOW_SHUTTER_OPEN,
        .icon_closed = MDI_SYMBOL_WINDOW_SHUTTER,
        .icon_closing = MDI_SYMBOL_ARROW_DOWN_BOX,
        .icon_opening = MDI_SYMBOL_ARROW_UP_BOX,
    },
    {
        .device_class = "window",
        .icon_default = MDI_SYMBOL_WINDOW_OPEN,
        .icon_closed = MDI_SYMBOL_WINDOW_CLOSED,
        .icon_closing = MDI_SYMBOL_ARROW_DOWN_BOX,
        .icon_opening = MDI_SYMBOL_ARROW_UP_BOX,
    },
};

static const cover_tile_icons_t *cover_find_tile_icons(
    const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot || !snapshot->device_class[0]) {
        return &COVER_TILE_DEFAULT_ICONS;
    }

    for (size_t i = 0;
         i < sizeof(COVER_TILE_ICONS) / sizeof(COVER_TILE_ICONS[0]);
         i++) {
        if (strcmp(snapshot->device_class,
                   COVER_TILE_ICONS[i].device_class) == 0) {
            return &COVER_TILE_ICONS[i];
        }
    }
    return &COVER_TILE_DEFAULT_ICONS;
}

static const char *cover_resolve_tile_icon(
    const ha_entity_snapshot_t *snapshot)
{
    const cover_tile_icons_t *icons = cover_find_tile_icons(snapshot);
    if (!snapshot) {
        return icons->icon_default;
    }
    if (strcmp(snapshot->state, "closed") == 0 && icons->icon_closed) {
        return icons->icon_closed;
    }
    if (strcmp(snapshot->state, "closing") == 0 && icons->icon_closing) {
        return icons->icon_closing;
    }
    if (strcmp(snapshot->state, "opening") == 0 && icons->icon_opening) {
        return icons->icon_opening;
    }
    return icons->icon_default;
}

static bool cover_handle_action(const entity_action_context_t *context)
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
    ha_entity_snapshot_t snapshot = {0};
    if (!store ||
        ha_entity_store_get(store, context->entity_id, &snapshot) != ESP_OK ||
        strcmp(snapshot.state, "unavailable") == 0 ||
        strcmp(snapshot.state, "unknown") == 0) {
        ESP_LOGW(TAG,
                 "Cannot toggle %s without an available state",
                 context->entity_id);
        return true;
    }

    const char *service = NULL;
    if (strcmp(snapshot.state, "open") == 0 ||
        strcmp(snapshot.state, "opening") == 0) {
        service = "close_cover";
    }
    else if (strcmp(snapshot.state, "closed") == 0) {
        service = "open_cover";
    }

    if (!service) {
        ESP_LOGW(TAG,
                 "Cannot toggle %s in state %s",
                 context->entity_id,
                 snapshot.state);
        return true;
    }

    esp_err_t err = app_ha_call_entity_service("cover",
                                               service,
                                               context->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "cover.%s for %s failed to send: %s",
                 service,
                 context->entity_id,
                 esp_err_to_name(err));
    }
    return true;
}

static void cover_more_info_close_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) {
        s_cover_detail_screen = NULL;
        s_cover_detail_entity_id[0] = '\0';
        s_cover_toggle_visual_valid = false;
        context->close(context->close_user_data);
    }
}

static uint32_t cover_get_supported_features(
    const entity_more_info_context_t *context)
{
    double value = 0;
    if (!context || !context->store ||
        ha_entity_store_get_attr_number(context->store,
                                        context->snapshot.entity_id,
                                        "supported_features",
                                        &value) != ESP_OK ||
        value < 0) {
        return 0;
    }
    return (uint32_t)value;
}

/* HA treats unknown differently from unavailable for an open/close-only
 * cover: unknown falls back to two explicit buttons, while unavailable makes
 * whichever control is shown inert. The shared entity helper intentionally
 * groups both states for tile rendering, so the detail page needs this narrower
 * predicate. */
static bool cover_snapshot_is_unavailable(
    const ha_entity_snapshot_t *snapshot)
{
    return !snapshot || snapshot->sync_status == HA_ENTITY_SYNC_REMOVED ||
           strcmp(snapshot->state, "unavailable") == 0;
}

static bool cover_get_assumed_state(
    const entity_more_info_context_t *context)
{
    bool assumed_state = false;
    return context && context->store &&
           ha_entity_store_get_attr_bool(context->store,
                                         context->snapshot.entity_id,
                                         "assumed_state",
                                         &assumed_state) == ESP_OK &&
           assumed_state;
}

static bool cover_supports_open_close_only(uint32_t features)
{
    const uint32_t excluded = COVER_FEATURE_STOP |
                              COVER_FEATURE_SET_POSITION |
                              COVER_FEATURE_OPEN_TILT |
                              COVER_FEATURE_CLOSE_TILT |
                              COVER_FEATURE_STOP_TILT |
                              COVER_FEATURE_SET_TILT_POS;
    return (features & (COVER_FEATURE_OPEN | COVER_FEATURE_CLOSE)) ==
               (COVER_FEATURE_OPEN | COVER_FEATURE_CLOSE) &&
           (features & excluded) == 0;
}

static bool cover_get_position(const entity_more_info_context_t *context,
                               int *position_out)
{
    double value = 0;
    if (!context || !context->store || !position_out ||
        ha_entity_store_get_attr_number(context->store,
                                        context->snapshot.entity_id,
                                        "current_position",
                                        &value) != ESP_OK) {
        return false;
    }
    if (value < 0) {
        value = 0;
    }
    else if (value > 100) {
        value = 100;
    }
    *position_out = (int)(value + 0.5);
    return true;
}

static bool cover_is_horizontal(const ha_entity_snapshot_t *snapshot)
{
    if (!snapshot) {
        return false;
    }
    return strcmp(snapshot->device_class, "awning") == 0 ||
           strcmp(snapshot->device_class, "curtain") == 0 ||
           strcmp(snapshot->device_class, "door") == 0 ||
           strcmp(snapshot->device_class, "gate") == 0;
}

static void cover_call_service(const entity_more_info_context_t *context,
                               const char *service,
                               int position)
{
    if (!context || !service || !context->snapshot.entity_id[0]) {
        return;
    }

    cJSON *service_data = NULL;
    if (position >= 0) {
        service_data = cJSON_CreateObject();
        if (!service_data ||
            !cJSON_AddNumberToObject(service_data, "position", position)) {
            cJSON_Delete(service_data);
            ESP_LOGE(TAG, "Unable to allocate cover service data");
            return;
        }
    }

    esp_err_t err = app_ha_call_entity_service_data("cover",
                                                    service,
                                                    context->snapshot.entity_id,
                                                    service_data);
    cJSON_Delete(service_data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "cover.%s for %s failed to send: %s",
                 service,
                 context->snapshot.entity_id,
                 esp_err_to_name(err));
    }
    else if (position >= 0) {
        ESP_LOGI(TAG,
                 "cover.%s sent for %s: position=%d",
                 service,
                 context->snapshot.entity_id,
                 position);
    }
    else {
        ESP_LOGI(TAG,
                 "cover.%s sent for %s",
                 service,
                 context->snapshot.entity_id);
    }
}

static void cover_action_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    lv_obj_t *button = lv_event_get_current_target_obj(event);
    if (!context || !button ||
        cover_snapshot_is_unavailable(&context->snapshot)) {
        return;
    }

    const cover_action_t action =
        (cover_action_t)(uintptr_t)lv_obj_get_user_data(button);
    switch (action) {
    case COVER_ACTION_OPEN:
        cover_call_service(context, "open_cover", -1);
        break;
    case COVER_ACTION_STOP:
        cover_call_service(context, "stop_cover", -1);
        break;
    case COVER_ACTION_CLOSE:
        cover_call_service(context, "close_cover", -1);
        break;
    default:
        break;
    }
}

static bool cover_snapshot_is_on(const ha_entity_snapshot_t *snapshot)
{
    return snapshot &&
           (strcmp(snapshot->state, "open") == 0 ||
            strcmp(snapshot->state, "opening") == 0 ||
            strcmp(snapshot->state, "closing") == 0);
}

static void cover_toggle_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    lv_obj_t *track = lv_event_get_current_target_obj(event);
    if (!context || !track ||
        cover_snapshot_is_unavailable(&context->snapshot)) {
        return;
    }

    lv_obj_t *knob = lv_obj_get_user_data(track);
    const bool current_on = s_cover_toggle_visual_valid
                                ? s_cover_toggle_visual_on
                                : cover_snapshot_is_on(&context->snapshot);
    const bool target_on = !current_on;
    cover_toggle_animate_knob(knob,
                             target_on ? COVER_TOGGLE_TOP_Y
                                       : COVER_TOGGLE_BOTTOM_Y);
    s_cover_toggle_visual_valid = true;
    s_cover_toggle_visual_on = target_on;

    cover_call_service(context,
                       target_on ? "open_cover" : "close_cover",
                       -1);
}

static int cover_position_to_slider_value(int position)
{
    if (position < 0) {
        position = 0;
    }
    else if (position > 100) {
        position = 100;
    }

    /* HA reserves one handle margin at each end. The active bar still reaches
     * the outer edge, while the handle travels only inside this safe area. */
    return COVER_POSITION_HEIGHT -
           ((position * COVER_POSITION_TRAVEL + 50) / 100);
}

static int cover_slider_value_to_position(const lv_obj_t *slider)
{
    int position = ((COVER_POSITION_HEIGHT - lv_slider_get_value(slider)) *
                    100 + (COVER_POSITION_TRAVEL / 2)) /
                   COVER_POSITION_TRAVEL;
    if (position < 0) {
        return 0;
    }
    if (position > 100) {
        return 100;
    }
    return position;
}

static int cover_position_hint_y_from_visual(int32_t visual_value)
{
    const int slider_top = (COVER_CONTROL_HEIGHT - COVER_POSITION_HEIGHT) / 2;
    const int handle_center = visual_value -
                              COVER_POSITION_KNOB_MARGIN -
                              (COVER_POSITION_KNOB_HEIGHT / 2);
    return slider_top + handle_center - 12;
}

static void cover_position_set_visual(void *object, int32_t visual_value)
{
    cover_position_view_t *view = object;
    if (!view) {
        return;
    }

    if (visual_value < COVER_POSITION_MIN_VALUE) {
        visual_value = COVER_POSITION_MIN_VALUE;
    }
    else if (visual_value > COVER_POSITION_HEIGHT) {
        visual_value = COVER_POSITION_HEIGHT;
    }
    view->visual_value = visual_value;

    if (view->active_bar && lv_obj_is_valid(view->active_bar)) {
        lv_obj_set_height(view->active_bar, visual_value);
    }
    if (view->knob && lv_obj_is_valid(view->knob)) {
        lv_obj_set_y(view->knob,
                     visual_value - COVER_POSITION_KNOB_MARGIN -
                         COVER_POSITION_KNOB_HEIGHT);
    }
    if (view->value_label && lv_obj_is_valid(view->value_label)) {
        lv_obj_align(view->value_label,
                     LV_ALIGN_TOP_LEFT,
                     42,
                     cover_position_hint_y_from_visual(visual_value));
    }
}

static void cover_position_animate_visual(cover_position_view_t *view,
                                          int32_t target_value)
{
    if (!view) {
        return;
    }

    if (target_value < COVER_POSITION_MIN_VALUE) {
        target_value = COVER_POSITION_MIN_VALUE;
    }
    else if (target_value > COVER_POSITION_HEIGHT) {
        target_value = COVER_POSITION_HEIGHT;
    }

    lv_anim_delete(view, cover_position_set_visual);
    if (view->visual_value == target_value) {
        cover_position_set_visual(view, target_value);
        return;
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view);
    lv_anim_set_exec_cb(&animation, cover_position_set_visual);
    lv_anim_set_values(&animation, view->visual_value, target_value);
    lv_anim_set_duration(&animation, COVER_POSITION_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}

static void cover_position_view_delete_event_cb(lv_event_t *event)
{
    cover_position_view_t *view = lv_event_get_user_data(event);
    if (!view) {
        return;
    }

    lv_anim_delete(view, cover_position_set_visual);
    lv_free(view);
}

static void cover_position_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    lv_obj_t *slider = lv_event_get_current_target_obj(event);
    cover_position_view_t *view = slider ? lv_obj_get_user_data(slider)
                                         : NULL;

    /* LVGL samples the pointer once more while processing RELEASED. A
     * capacitive touch point can drift as the finger leaves the glass, so use
     * the last value observed during PRESSING as the committed position. */
    if (slider && view && view->has_last_drag_value) {
        lv_slider_set_value(slider, view->last_drag_value, LV_ANIM_OFF);
        lv_anim_delete(view, cover_position_set_visual);
        cover_position_set_visual(view, view->last_drag_value);
        s_cover_position_visual_valid = true;
        s_cover_position_visual_value = view->last_drag_value;
        view->has_last_drag_value = false;
    }
    if (view && view->value_label &&
        lv_obj_is_valid(view->value_label)) {
        lv_obj_add_flag(view->value_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (!context || !slider ||
        cover_snapshot_is_unavailable(&context->snapshot)) {
        return;
    }
    const int position = cover_slider_value_to_position(slider);
    cover_call_service(context, "set_cover_position", position);
}

static void cover_position_cancel_event_cb(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_current_target_obj(event);
    if (!slider) {
        return;
    }

    cover_position_view_t *view = lv_obj_get_user_data(slider);
    if (view && view->value_label && lv_obj_is_valid(view->value_label)) {
        lv_obj_add_flag(view->value_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (view) {
        view->has_last_drag_value = false;
    }
}

static void cover_position_drag_sample_event_cb(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_current_target_obj(event);
    cover_position_view_t *view = slider ? lv_obj_get_user_data(slider)
                                         : NULL;
    if (!slider || !view) {
        return;
    }

    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
        view->has_last_drag_value = false;
    }
    else if (lv_slider_is_dragged(slider)) {
        view->last_drag_value = lv_slider_get_value(slider);
        view->has_last_drag_value = true;
    }
}

static void cover_position_value_event_cb(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_current_target_obj(event);
    if (!slider) {
        return;
    }

    /* Pointer coordinates cover the complete LVGL track, but HA reserves the
     * top handle spacing. Clamp that small dead zone so the active segment and
     * handle remain visible at 100%. */
    if (lv_slider_get_value(slider) < COVER_POSITION_MIN_VALUE) {
        lv_slider_set_value(slider,
                            COVER_POSITION_MIN_VALUE,
                            LV_ANIM_OFF);
    }

    cover_position_view_t *view = lv_obj_get_user_data(slider);
    if (!view || !view->value_label ||
        !lv_obj_is_valid(view->value_label)) {
        return;
    }

    lv_obj_remove_flag(view->value_label, LV_OBJ_FLAG_HIDDEN);
    const int position = cover_slider_value_to_position(slider);
    char value_text[16];
    snprintf(value_text, sizeof(value_text), "%d%%", position);
    lv_label_set_text(view->value_label, value_text);

    const int32_t visual_value = lv_slider_get_value(slider);
    if (lv_slider_is_dragged(slider)) {
        /* Pointer updates must track the finger directly. Restarting the
         * 180 ms easing animation for every value event makes the handle lag
         * behind continuous touch input. */
        lv_anim_delete(view, cover_position_set_visual);
        cover_position_set_visual(view, visual_value);
    }
    else {
        cover_position_animate_visual(view, visual_value);
    }
    s_cover_position_visual_valid = true;
    s_cover_position_visual_value = visual_value;
}

static void cover_preset_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    lv_obj_t *button = lv_event_get_current_target_obj(event);
    if (!context || !button ||
        cover_snapshot_is_unavailable(&context->snapshot)) {
        return;
    }
    int position = (int)(uintptr_t)lv_obj_get_user_data(button);
    cover_call_service(context, "set_cover_position", position);
}

static void cover_refresh_async(void *user_data)
{
    const entity_more_info_context_t *context = user_data;
    if (context && context->store && context->snapshot.entity_id[0]) {
        more_info_screen_refresh_entity(context->store,
                                        context->snapshot.entity_id);
    }
}

static void cover_mode_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    lv_obj_t *button = lv_event_get_current_target_obj(event);
    if (!context || !button) {
        return;
    }
    cover_detail_mode_t mode =
        (cover_detail_mode_t)(uintptr_t)lv_obj_get_user_data(button);
    if (mode == s_cover_detail_mode) {
        return;
    }
    s_cover_detail_mode = mode;
    lv_async_call(cover_refresh_async, (void *)context);
}

static lv_obj_t *cover_more_info_build_page(
    const entity_more_info_context_t *context)
{
    lv_obj_t *screen = context->screen;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COVER_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(screen, 0, 0);

    lv_obj_t *header = lv_obj_create(screen);
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
                        cover_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_height(title, lv_font_get_line_height(&ha_font_misan_medium_20));

    lv_obj_t *close_hitbox = lv_obj_create(context->overlay);
    lv_obj_remove_style_all(close_hitbox);
    lv_obj_remove_flag(close_hitbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(close_hitbox, 50, 50);
    lv_obj_align(close_hitbox, LV_ALIGN_TOP_LEFT, 20, 12);
    lv_obj_add_event_cb(close_hitbox,
                        cover_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(content, COVER_DETAIL_WIDTH);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(content, 0, 0);
    lv_obj_set_style_pad_top(content, 4, 0);
    lv_obj_set_style_pad_bottom(content, 8, 0);
    lv_obj_set_style_pad_gap(content, 8, 0);
    return content;
}

static void cover_build_state_block(
    lv_obj_t *parent,
    const entity_more_info_context_t *context)
{
    lv_obj_t *block = lv_obj_create(parent);
    lv_obj_remove_style_all(block);
    lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(block, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(block, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(block,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(block, 0, 0);
    lv_obj_set_style_pad_gap(block, 2, 0);

    char state_text[64] = {0};
    entity_handler_copy_snapshot_state(state_text,
                                       sizeof(state_text),
                                       &context->snapshot);
    int position = 0;
    if (!cover_snapshot_is_unavailable(&context->snapshot) &&
        strcmp(context->snapshot.state, "unknown") != 0 &&
        cover_get_position(context, &position) &&
        position > 0 && position < 100) {
        size_t used = strlen(state_text);
        snprintf(state_text + used,
                 sizeof(state_text) - used,
                 " · %d%%",
                 position);
    }

    lv_obj_t *state = lv_label_create(block);
    lv_label_set_text(state, state_text);
    lv_obj_set_style_text_font(state, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(state, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(state, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(state,
                    LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_medium_20));

    char time_text[48] = {0};
    time_t changed_epoch = 0;
    const bool has_time =
        context->snapshot.last_changed[0] &&
        ha_datetime_format_relative_now(context->snapshot.last_changed,
                                        time_text,
                                        sizeof(time_text)) == ESP_OK &&
        ha_datetime_parse_epoch(context->snapshot.last_changed,
                                &changed_epoch) == ESP_OK;
    lv_obj_t *time_label = lv_label_create(block);
    lv_label_set_text(time_label, has_time ? time_text : "");
    lv_obj_set_style_text_font(time_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(time_label,
                    LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_regular_16));
    if (has_time) {
        ha_relative_time_label_bind(time_label, changed_epoch);
    }
    else {
        lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static bool cover_action_is_enabled(
    const entity_more_info_context_t *context,
    cover_action_t action)
{
    if (cover_snapshot_is_unavailable(&context->snapshot)) {
        return false;
    }
    if (action == COVER_ACTION_STOP) {
        return true;
    }

    if (cover_get_assumed_state(context)) {
        return true;
    }

    int position = 0;
    const bool has_position = cover_get_position(context, &position);
    if (action == COVER_ACTION_OPEN) {
        return strcmp(context->snapshot.state, "opening") != 0 &&
               !(has_position ? position >= 100
                              : strcmp(context->snapshot.state, "open") == 0);
    }
    return strcmp(context->snapshot.state, "closing") != 0 &&
           !(has_position ? position <= 0
                          : strcmp(context->snapshot.state, "closed") == 0);
}

static void cover_build_action_button(
    lv_obj_t *parent,
    const entity_more_info_context_t *context,
    cover_action_t action,
    const char *icon)
{
    const bool enabled = cover_action_is_enabled(context, action);
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_user_data(button, (void *)(uintptr_t)action);
    lv_obj_set_size(button, COVER_BUTTON_WIDTH, COVER_BUTTON_HEIGHT);
    lv_obj_set_style_radius(button, COVER_BUTTON_RADIUS, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(COVER_CONTROL_BG), 0);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(COVER_CONTROL_PRESSED),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(COVER_DISABLED_BG),
                              LV_STATE_DISABLED);
    lv_obj_add_event_cb(button,
                        cover_action_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);
    if (!enabled) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, icon);
    lv_obj_set_style_text_font(label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(label,
                                lv_color_hex(COVER_DISABLED_FG),
                                LV_STATE_DISABLED);
    lv_obj_center(label);
    if (!enabled) {
        lv_obj_set_style_text_color(label,
                                    lv_color_hex(COVER_DISABLED_FG),
                                    0);
    }
}

static void cover_build_button_control(
    lv_obj_t *parent,
    const entity_more_info_context_t *context,
    uint32_t features)
{
    lv_obj_t *group = lv_obj_create(parent);
    lv_obj_remove_style_all(group);
    lv_obj_remove_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(group, COVER_BUTTON_WIDTH, COVER_CONTROL_HEIGHT);
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(group, 0, 0);
    lv_obj_set_style_pad_gap(group, 10, 0);

    const bool horizontal = cover_is_horizontal(&context->snapshot);
    if (features & COVER_FEATURE_OPEN) {
        cover_build_action_button(group,
                                  context,
                                  COVER_ACTION_OPEN,
                                  horizontal ? MDI_SYMBOL_ARROW_EXPAND_HORIZONTAL
                                             : MDI_SYMBOL_ARROW_UP);
    }
    if (features & COVER_FEATURE_STOP) {
        cover_build_action_button(group,
                                  context,
                                  COVER_ACTION_STOP,
                                  MDI_SYMBOL_STOP);
    }
    if (features & COVER_FEATURE_CLOSE) {
        cover_build_action_button(group,
                                  context,
                                  COVER_ACTION_CLOSE,
                                  horizontal ? MDI_SYMBOL_ARROW_COLLAPSE_HORIZONTAL
                                             : MDI_SYMBOL_ARROW_DOWN);
    }
}

/* Home Assistant uses a reversed vertical switch when OPEN and CLOSE are the
 * only supported Cover features. Open/opening/closing park the knob at the top;
 * closed parks it at the bottom. Its web component updates checked immediately
 * and transitions the knob for 180 ms; Entity Store updates remain authoritative
 * and also animate when the state was changed by another client. */
static void cover_toggle_set_knob_y(void *object, int32_t y)
{
    lv_obj_t *knob = object;
    if (knob && lv_obj_is_valid(knob)) {
        lv_obj_set_y(knob, y);
    }
}

static void cover_toggle_animate_knob(lv_obj_t *knob, int32_t target_y)
{
    if (!knob || !lv_obj_is_valid(knob)) {
        return;
    }

    const int32_t current_y = lv_obj_get_y(knob);
    if (current_y == target_y) {
        return;
    }

    lv_anim_delete(knob, cover_toggle_set_knob_y);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, knob);
    lv_anim_set_exec_cb(&animation, cover_toggle_set_knob_y);
    lv_anim_set_values(&animation, current_y, target_y);
    lv_anim_set_duration(&animation, COVER_TOGGLE_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}

static void cover_build_open_close_toggle(
    lv_obj_t *parent,
    const entity_more_info_context_t *context)
{
    const bool unavailable = cover_snapshot_is_unavailable(&context->snapshot);
    const bool is_on = cover_snapshot_is_on(&context->snapshot);
    const bool animate = !unavailable && s_cover_toggle_visual_valid &&
                         s_cover_toggle_visual_on != is_on;
    const bool initial_on = animate ? s_cover_toggle_visual_on : is_on;
    const int32_t initial_y = initial_on ? COVER_TOGGLE_TOP_Y
                                         : COVER_TOGGLE_BOTTOM_Y;
    const int32_t target_y = is_on ? COVER_TOGGLE_TOP_Y
                                    : COVER_TOGGLE_BOTTOM_Y;
    const uint32_t state_color = is_on ? COVER_TOGGLE_OPEN
                                       : COVER_TOGGLE_CLOSED;

    lv_obj_t *track = lv_obj_create(parent);
    lv_obj_remove_style_all(track);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(track,
                    COVER_TOGGLE_TRACK_WIDTH,
                    COVER_TOGGLE_TRACK_HEIGHT);
    lv_obj_set_style_radius(track, COVER_TOGGLE_RADIUS, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_set_style_bg_color(track,
                              lv_color_hex(unavailable
                                               ? COVER_TOGGLE_DISABLED_TRACK
                                               : state_color),
                              0);
    lv_obj_set_style_bg_opa(track,
                            unavailable ? LV_OPA_COVER
                                        : COVER_TOGGLE_TRACK_OPA,
                            0);
    lv_obj_set_style_bg_opa(track,
                            unavailable ? LV_OPA_COVER
                                        : COVER_TOGGLE_PRESSED_OPA,
                            LV_STATE_PRESSED);

    if (unavailable) {
        lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(track, LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(track,
                            cover_toggle_event_cb,
                            LV_EVENT_CLICKED,
                            (void *)context);
    }

    lv_obj_t *knob = lv_obj_create(track);
    lv_obj_remove_style_all(knob);
    lv_obj_remove_flag(knob, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(knob,
                    COVER_TOGGLE_KNOB_WIDTH,
                    COVER_TOGGLE_KNOB_HEIGHT);
    lv_obj_set_style_radius(knob, COVER_TOGGLE_RADIUS, 0);
    lv_obj_set_style_border_width(knob, 0, 0);
    lv_obj_set_style_bg_color(knob,
                              lv_color_hex(unavailable
                                               ? COVER_TOGGLE_DISABLED_KNOB
                                               : state_color),
                              0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_set_pos(knob, COVER_TOGGLE_INSET, initial_y);
    lv_obj_set_user_data(track, knob);

    const cover_tile_icons_t *icons = cover_find_tile_icons(&context->snapshot);
    const char *icon = is_on || !icons->icon_closed
                           ? icons->icon_default
                           : icons->icon_closed;
    lv_obj_t *icon_label = lv_label_create(knob);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(icon_label,
                                lv_color_hex(unavailable ? 0x999999
                                                         : 0xFFFFFF),
                                0);
    lv_obj_center(icon_label);

    if (animate) {
        lv_obj_update_layout(track);
        cover_toggle_animate_knob(knob, target_y);
    }

    s_cover_toggle_visual_valid = !unavailable;
    s_cover_toggle_visual_on = is_on;
}

static void cover_build_position_control(
    lv_obj_t *parent,
    const entity_more_info_context_t *context)
{
    const bool unavailable = cover_snapshot_is_unavailable(
        &context->snapshot);
    int position = 0;
    cover_get_position(context, &position);

    lv_obj_t *control = lv_obj_create(parent);
    lv_obj_remove_style_all(control);
    lv_obj_remove_flag(control, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(control, LV_PCT(100), COVER_CONTROL_HEIGHT);

    cover_position_view_t *view = lv_malloc(sizeof(*view));
    if (!view) {
        ESP_LOGE(TAG, "Failed to allocate cover position view");
        lv_obj_delete(control);
        return;
    }
    memset(view, 0, sizeof(*view));
    lv_obj_add_event_cb(control,
                        cover_position_view_delete_event_cb,
                        LV_EVENT_DELETE,
                        view);

    const int32_t target_visual = cover_position_to_slider_value(position);
    const bool animate_from_previous = !unavailable &&
                                       s_cover_position_visual_valid &&
                                       s_cover_position_visual_value !=
                                           target_visual;
    const int32_t initial_visual = animate_from_previous
                                       ? s_cover_position_visual_value
                                       : target_visual;

    /* These ordinary objects render the control without LVGL's bar masking.
     * Keeping the actual slider transparent avoids the large ARGB8888 layer
     * otherwise created when inner and outer radii are different. */
    lv_obj_t *track = lv_obj_create(control);
    lv_obj_remove_style_all(track);
    lv_obj_remove_flag(track,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(track, COVER_POSITION_WIDTH, COVER_POSITION_HEIGHT);
    lv_obj_align(track, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(track, 20, 0);
    /* Clip only the small top/bottom corner strips to the 20 px outer radius.
     * This preserves the track outline when the 12 px active bar is full,
     * without recreating the former full-height ARGB8888 layer. */
    lv_obj_set_style_clip_corner(track, true, 0);
    lv_obj_set_style_bg_color(track,
                              lv_color_hex(unavailable ? COVER_DISABLED_BG
                                                       : COVER_POSITION_TRACK),
                              0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);

    view->active_bar = lv_obj_create(track);
    lv_obj_remove_style_all(view->active_bar);
    lv_obj_remove_flag(view->active_bar,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(view->active_bar, 0, 0);
    lv_obj_set_width(view->active_bar, LV_PCT(100));
    lv_obj_set_style_radius(view->active_bar,
                            COVER_POSITION_INNER_RADIUS,
                            0);
    lv_obj_set_style_bg_color(
        view->active_bar,
        lv_color_hex(unavailable ? COVER_DISABLED_FG
                                 : COVER_POSITION_ACTIVE),
        0);
    lv_obj_set_style_bg_opa(view->active_bar, LV_OPA_COVER, 0);

    view->knob = lv_obj_create(track);
    lv_obj_remove_style_all(view->knob);
    lv_obj_remove_flag(view->knob,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(view->knob,
                    COVER_POSITION_KNOB_WIDTH,
                    COVER_POSITION_KNOB_HEIGHT);
    lv_obj_set_x(view->knob,
                 (COVER_POSITION_WIDTH - COVER_POSITION_KNOB_WIDTH) / 2);
    lv_obj_set_style_radius(view->knob, 2, 0);
    lv_obj_set_style_bg_color(view->knob,
                              lv_color_hex(unavailable ? COVER_DISABLED_FG
                                                       : 0xFFFFFF),
                              0);
    lv_obj_set_style_bg_opa(view->knob, LV_OPA_COVER, 0);

    char value_text[16];
    snprintf(value_text, sizeof(value_text), "%d%%", position);
    lv_obj_t *value_label = lv_label_create(control);
    lv_label_set_text(value_label, value_text);
    lv_obj_set_style_text_font(value_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(value_label,
                                lv_color_hex(unavailable ? COVER_DISABLED_FG
                                                         : 0xDADADA),
                                0);
    lv_obj_set_style_bg_color(value_label, lv_color_hex(0x252525), 0);
    lv_obj_set_style_bg_opa(value_label, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(value_label, 8, 0);
    lv_obj_set_style_pad_hor(value_label, 6, 0);
    lv_obj_set_style_pad_ver(value_label, 3, 0);
    lv_obj_align(value_label,
                 LV_ALIGN_TOP_LEFT,
                 42,
                 cover_position_hint_y_from_visual(initial_visual));
    lv_obj_add_flag(value_label, LV_OBJ_FLAG_HIDDEN);
    view->value_label = value_label;
    view->visual_value = initial_visual;
    cover_position_set_visual(view, initial_visual);

    lv_obj_t *slider = lv_slider_create(control);
    lv_obj_remove_style_all(slider);
    lv_obj_set_size(slider, COVER_POSITION_WIDTH, COVER_POSITION_HEIGHT);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(slider, LV_OBJ_FLAG_SCROLLABLE);
    lv_slider_set_orientation(slider, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider, COVER_POSITION_HEIGHT, 0);
    lv_slider_set_value(slider, target_visual, LV_ANIM_OFF);
    /* Explicit transparent styles also keep all slider parts at radius zero,
     * preventing LVGL from creating a masked intermediate layer. */
    lv_obj_set_style_radius(slider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    if (unavailable) {
        lv_obj_add_state(slider, LV_STATE_DISABLED);
    }

    lv_obj_set_user_data(slider, view);
    lv_obj_add_event_cb(slider,
                        cover_position_drag_sample_event_cb,
                        LV_EVENT_PRESSED,
                        NULL);
    lv_obj_add_event_cb(slider,
                        cover_position_drag_sample_event_cb,
                        LV_EVENT_PRESSING,
                        NULL);
    lv_obj_add_event_cb(slider,
                        cover_position_value_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);
    lv_obj_add_event_cb(slider,
                        cover_position_event_cb,
                        LV_EVENT_RELEASED,
                        (void *)context);
    lv_obj_add_event_cb(slider,
                        cover_position_cancel_event_cb,
                        LV_EVENT_PRESS_LOST,
                        NULL);

    if (animate_from_previous) {
        cover_position_animate_visual(view, target_visual);
    }
    s_cover_position_visual_valid = !unavailable;
    s_cover_position_visual_value = target_visual;
}

static lv_obj_t *cover_build_mode_button(
    lv_obj_t *parent,
    const entity_more_info_context_t *context,
    cover_detail_mode_t mode,
    const char *icon)
{
    const bool selected = s_cover_detail_mode == mode;
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_user_data(button, (void *)(uintptr_t)mode);
    lv_obj_set_size(button, 48, 48);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(selected ? 0xFFFFFF
                                                    : COVER_CONTROL_BG),
                              0);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(COVER_CONTROL_PRESSED),
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(button,
                        cover_mode_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, icon);
    lv_obj_set_style_text_font(label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(label,
                                lv_color_hex(selected ? 0x202020 : 0xC8C8C8),
                                0);
    lv_obj_center(label);
    return button;
}

static void cover_build_mode_switch(
    lv_obj_t *parent,
    const entity_more_info_context_t *context)
{
    lv_obj_t *group = lv_obj_create(parent);
    lv_obj_remove_style_all(group);
    lv_obj_remove_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(group, COVER_MODE_WIDTH, COVER_MODE_HEIGHT);
    lv_obj_set_style_radius(group, COVER_MODE_HEIGHT / 2, 0);
    lv_obj_set_style_bg_color(group, lv_color_hex(COVER_CONTROL_BG), 0);
    lv_obj_set_style_bg_opa(group, LV_OPA_COVER, 0);
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(group,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(group, 0, 0);
    lv_obj_set_style_pad_gap(group, 4, 0);
    cover_build_mode_button(group,
                            context,
                            COVER_DETAIL_MODE_POSITION,
                            MDI_SYMBOL_MENU);
    cover_build_mode_button(group,
                            context,
                            COVER_DETAIL_MODE_BUTTONS,
                            MDI_SYMBOL_SWAP_VERTICAL);
}

static void cover_build_presets(
    lv_obj_t *parent,
    const entity_more_info_context_t *context)
{
    static const int positions[] = {0, 25, 75, 100};
    const bool unavailable = cover_snapshot_is_unavailable(
        &context->snapshot);
    int current_position = -1;
    cover_get_position(context, &current_position);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_PCT(100), COVER_PRESET_HEIGHT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 0, 0);

    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        const bool selected = !unavailable && current_position == positions[i];
        lv_obj_t *button = lv_button_create(row);
        lv_obj_set_user_data(button, (void *)(uintptr_t)positions[i]);
        lv_obj_set_size(button, 62, COVER_PRESET_HEIGHT);
        lv_obj_set_style_radius(button, COVER_PRESET_HEIGHT / 2, 0);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_set_style_bg_color(button,
                                  lv_color_hex(selected ? 0xFFFFFF
                                                        : COVER_CONTROL_BG),
                                  0);
        lv_obj_set_style_bg_color(button,
                                  lv_color_hex(COVER_CONTROL_PRESSED),
                                  LV_STATE_PRESSED);
        lv_obj_add_event_cb(button,
                            cover_preset_event_cb,
                            LV_EVENT_CLICKED,
                            (void *)context);
        if (unavailable) {
            lv_obj_add_state(button, LV_STATE_DISABLED);
        }

        char text[8];
        snprintf(text, sizeof(text), "%d%%", positions[i]);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &ha_font_misan_regular_16, 0);
        lv_obj_set_style_text_color(label,
                                    lv_color_hex(unavailable
                                                     ? COVER_DISABLED_FG
                                                     : selected ? 0x202020
                                                                : 0xDADADA),
                                    0);
        lv_obj_center(label);
    }
}

static esp_err_t cover_render_tile(ha_entity_store_t *store,
                                   const ha_entity_snapshot_t *snapshot,
                                   entity_display_model_t *model)
{
    if (!store || !snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = cover_resolve_tile_icon(snapshot);
    model->background_color = lv_color_hex(0x1E2228);
    model->title_color = lv_color_hex(0xFFFFFF);
    model->state_color = lv_color_hex(0x666666);
    model->icon_color = lv_color_hex(0x9E9E9E);
    model->icon_circle_background_color = lv_color_hex(0x363636);
    model->has_ripple_color = true;
    model->ripple_color = lv_color_hex(0xD9D9D9);

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon_color = lv_color_hex(0x6F6F6F);
        model->icon_circle_background_color = lv_color_hex(0x2D2D2D);
        return ESP_OK;
    }

    if (strcmp(snapshot->state, "closed") != 0) {
        model->icon_color = lv_color_hex(0x926BC7);
        model->icon_circle_background_color = lv_color_hex(0x342C3E);
    }

    double position = 0;
    if (ha_entity_store_get_attr_number(store,
                                        snapshot->entity_id,
                                        "current_position",
                                        &position) == ESP_OK) {
        if (position < 0) {
            position = 0;
        }
        else if (position > 100) {
            position = 100;
        }

        char state_text[HA_ENTITY_STORE_STATE_MAX] = {0};
        entity_handler_copy_state_text(state_text,
                                       sizeof(state_text),
                                       snapshot->state);
        snprintf(model->state_text,
                 sizeof(model->state_text),
                 "%s - %.0f%%",
                 state_text,
                 position);
        return ESP_OK;
    }

    entity_handler_copy_snapshot_state(model->state_text,
                                       sizeof(model->state_text),
                                       snapshot);
    return ESP_OK;
}

static esp_err_t cover_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->store || !context->close) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t features = cover_get_supported_features(context);
    const bool supports_position = features & COVER_FEATURE_SET_POSITION;
    const bool supports_buttons = features & (COVER_FEATURE_OPEN |
                                               COVER_FEATURE_CLOSE |
                                               COVER_FEATURE_STOP);
    const bool supports_open_close_only =
        cover_supports_open_close_only(features);
    const bool use_open_close_toggle =
        supports_open_close_only && !cover_get_assumed_state(context) &&
        strcmp(context->snapshot.state, "unknown") != 0;

    if (s_cover_detail_screen != context->screen ||
        strcmp(s_cover_detail_entity_id,
               context->snapshot.entity_id) != 0) {
        s_cover_toggle_visual_valid = false;
        s_cover_position_visual_valid = false;
        s_cover_detail_screen = context->screen;
        snprintf(s_cover_detail_entity_id,
                 sizeof(s_cover_detail_entity_id),
                 "%s",
                 context->snapshot.entity_id);
        s_cover_detail_mode = supports_buttons
                                  ? COVER_DETAIL_MODE_BUTTONS
                                  : COVER_DETAIL_MODE_POSITION;
    }
    if (!supports_position) {
        s_cover_detail_mode = COVER_DETAIL_MODE_BUTTONS;
    }
    else if (!supports_buttons) {
        s_cover_detail_mode = COVER_DETAIL_MODE_POSITION;
    }
    if (!use_open_close_toggle) {
        s_cover_toggle_visual_valid = false;
    }

    lv_obj_t *content = cover_more_info_build_page(context);
    cover_build_state_block(content, context);

    if (s_cover_detail_mode == COVER_DETAIL_MODE_POSITION &&
        supports_position) {
        cover_build_position_control(content, context);
    }
    else if (use_open_close_toggle) {
        cover_build_open_close_toggle(content, context);
    }
    else if (supports_buttons) {
        cover_build_button_control(content, context, features);
    }

    if (supports_position && supports_buttons) {
        cover_build_mode_switch(content, context);
    }
    if (supports_position) {
        cover_build_presets(content, context);
    }

    return ESP_OK;
}

const entity_handler_t cover_entity_handler = {
    .domain = "cover",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_WINDOW_OPEN,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_NOTHING },
    },
    .handle_action = cover_handle_action,
    .render_tile = cover_render_tile,
    .render_more_info = cover_render_more_info,
};
