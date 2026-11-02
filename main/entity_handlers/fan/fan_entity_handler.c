#include "esp_log.h"
#include "fan_entity_handler.h"
#include "app.h"
#include "assets/ha_ui_assets.h"
#include <lvgl_private.h>
#include "assets/mdi_symbol_def.h"
#include "entity_handlers/ha_datetime_format.h"
#include "screen/components/ha_relative_time_label.h"
#include "bsp_lcd.h"
#include "i18n.h"
#include "ui_i18n.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "fan_entity_handler";

static void fan_label_set_display_text(lv_obj_t *label, const char *text)
{
    char display[64];
    i18n_display_copy(display, sizeof(display), text);
    lv_label_set_text(label, display);
}

/* ---------------------------------------------------------------------------
 * Design constants (320x480 panel).
 * ------------------------------------------------------------------------- */

#define FAN_TILE_BG              0x1E2228
#define FAN_TILE_ICON_ON         0x00bcd4
#define FAN_TILE_ICON_OFF        0x9e9e9e
#define FAN_TILE_CIRCLE_ON       0x163c41
#define FAN_TILE_CIRCLE_OFF      0x363636
#define FAN_TILE_TITLE           0xFFFFFF
#define FAN_TILE_STATE           0x666666
#define FAN_TILE_DISABLED_BG     0x242424
#define FAN_TILE_DISABLED_CIRCLE 0x3A3A3A
#define FAN_TILE_DISABLED_ICON   0x6f6f6f

#define FAN_DETAIL_CONTENT_WIDTH 280
#define FAN_CONTROL_GAP          16
#define FAN_ROW_GAP              12

/* Main control positions match the 320x480 design. */
#define FAN_VSLIDER_X              120
#define FAN_VSLIDER_Y              142
#define FAN_VSLIDER_WIDTH          80
#define FAN_VSLIDER_HEIGHT         196
#define FAN_VSLIDER_RADIUS         20
#define FAN_VSLIDER_COLOR          0x08AECA
#define FAN_VSLIDER_BG_OPA         LV_OPA_20
#define FAN_VSLIDER_BG             FAN_VSLIDER_COLOR
#define FAN_VSLIDER_INDICATOR_W    52
#define FAN_VSLIDER_INDICATOR_H    4
#define FAN_VSLIDER_INDICATOR_R    14
#define FAN_VSLIDER_INDICATOR_CLR  0xFFFFFF
#define FAN_VSLIDER_INDICATOR_TOP  11
#define FAN_VSLIDER_TIP_W          56
#define FAN_VSLIDER_TIP_H          26

/* Capability-specific main controls from the 320x480 design. */
#define FAN_SWITCH_X                117
#define FAN_SWITCH_Y                142
#define FAN_SWITCH_W                88
#define FAN_SWITCH_H                230
#define FAN_SWITCH_INSET            6
#define FAN_SWITCH_KNOB_W           76
#define FAN_SWITCH_KNOB_W_LANDSCAPE 76
#define FAN_SWITCH_KNOB_H           109
#define FAN_SWITCH_KNOB_H_LANDSCAPE 79
#define FAN_SWITCH_RADIUS           20
#define FAN_SPEED_SWITCH_X          116
#define FAN_SPEED_SWITCH_Y          142
#define FAN_SPEED_SWITCH_W          88
#define FAN_SPEED_SWITCH_H          298
#define FAN_SPEED_OPTION_W          76
#define FAN_SPEED_OPTION_W_LANDSCAPE 100
#define FAN_SPEED_OPTION_H          71
#define FAN_SPEED_OPTION_H_LANDSCAPE 40
#define FAN_CONTROL_ON_COLOR        0x08AECA
#define FAN_CONTROL_OFF_COLOR       0x999999
#define FAN_CONTROL_INACTIVE_FG     0x666666

/* Speed button dimensions (discrete speeds). */
#define FAN_SPEED_BTN_SIZE       56
#define FAN_SPEED_BTN_RADIUS     12
#define FAN_SPEED_BTN_BG         0x1F1F1F
#define FAN_SPEED_BTN_ACTIVE_BG  0x00bcd4
#define FAN_SPEED_BTN_ACTIVE_FG  0xFFFFFF
#define FAN_SPEED_BTN_FG         0x999999
#define FAN_SPEED_BTN_ROW_GAP    10
#define FAN_HA_SPEED_COUNT_MAX    6

/* Power Toggle Button */
#define FAN_PWR_BTN_SIZE           48
#define FAN_PWR_BTN_RADIUS         30
#define FAN_PWR_BTN_X              136
#define FAN_PWR_BTN_Y              346
#define FAN_PWR_BTN_BG             0x999999
#define FAN_PWR_BTN_BG_OPA         LV_OPA_20
#define FAN_PWR_BTN_ICON           0xFFFFFF

/* Bottom Function Bar */
#define FAN_BOTTOM_BAR_WIDTH       280
#define FAN_BOTTOM_BAR_HEIGHT      50
#define FAN_BOTTOM_BAR_X           20
#define FAN_BOTTOM_BAR_Y           410
#define FAN_BOTTOM_BAR_GAP         5
#define FAN_BOTTOM_ITEM_RADIUS     12
#define FAN_BOTTOM_ITEM_PAD_VER    4
#define FAN_BOTTOM_ITEM_PAD_HOR    10
#define FAN_BOTTOM_ICON_SIZE       20
#define FAN_BOTTOM_FONT            &ha_font_misan_regular_14 /* Ensure this font exists */
#define FAN_BOTTOM_TEXT_COLOR      0xFFFFFF
#define FAN_BOTTOM_BG_DEFAULT      0x1F1F1F
#define FAN_BOTTOM_BG_ACTIVE       0x303030
#define FAN_BOTTOM_POPUP_BG        0x24272D
#define FAN_BOTTOM_POPUP_ROW_H     40
#define FAN_BOTTOM_POPUP_MAX_H     136

/* Dynamic bottom bar: 1-item (centered) or 2-item (22.5 from sides) */
#define FAN_BOTTOM_ITEM_SINGLE_W   135
#define FAN_BOTTOM_ITEM_SINGLE_X   ((320 - FAN_BOTTOM_ITEM_SINGLE_W) / 2)
#define FAN_BOTTOM_ITEM_DUAL_X     22

/* ---------------------------------------------------------------------------
 * Landscape (480x320) layout constants
 * --------------------------------------------------------------------------- */

/* State block - landscape */
#define FAN_STATE_BLOCK_X_LANDSCAPE       168
#define FAN_STATE_BLOCK_Y_LANDSCAPE       66
#define FAN_STATE_BLOCK_W_LANDSCAPE       145

/* Horizontal slider (landscape) - slider+switch type */
#define FAN_HSLIDER_X_LANDSCAPE           205
#define FAN_HSLIDER_Y_LANDSCAPE           126
#define FAN_HSLIDER_W_LANDSCAPE           70
#define FAN_HSLIDER_H_LANDSCAPE           174
#define FAN_HSLIDER_RADIUS_LANDSCAPE      12
#define FAN_HSLIDER_INDICATOR_W_LANDSCAPE 46
#define FAN_HSLIDER_INDICATOR_H_LANDSCAPE 4
#define FAN_HSLIDER_INDICATOR_R_LANDSCAPE 14
#define FAN_HSLIDER_INDICATOR_TOP_LANDSCAPE 8
#define FAN_HSLIDER_TIP_W_LANDSCAPE       56
#define FAN_HSLIDER_TIP_H_LANDSCAPE       26

/* Power button (landscape - slider+switch type) */
#define FAN_PWR_BTN_X_LANDSCAPE           127
#define FAN_PWR_BTN_Y_LANDSCAPE           189
#define FAN_PWR_BTN_SIZE_LANDSCAPE        48
#define FAN_PWR_BTN_RADIUS_LANDSCAPE      30

/* Icon switch / Labeled switch (landscape) - angle 90 deg */
#define FAN_SWITCH_X_LANDSCAPE            205
#define FAN_SWITCH_Y_LANDSCAPE            126
#define FAN_SWITCH_W_LANDSCAPE            88
#define FAN_SWITCH_H_LANDSCAPE            170
#define FAN_SWITCH_RADIUS_LANDSCAPE       20

/* Speed buttons (landscape) - angle 90 deg */
#define FAN_SPEED_SWITCH_X_LANDSCAPE      184
#define FAN_SPEED_SWITCH_Y_LANDSCAPE      126
#define FAN_SPEED_SWITCH_W_LANDSCAPE      112
#define FAN_SPEED_SWITCH_H_LANDSCAPE      172

/* Bottom bar (landscape) - vertical on right side */
#define FAN_BOTTOM_BAR_X_LANDSCAPE        305
#define FAN_BOTTOM_BAR_W_LANDSCAPE        103
#define FAN_BOTTOM_BAR_GAP_LANDSCAPE      5
#define FAN_BOTTOM_BAR_Y3_LANDSCAPE       133
#define FAN_BOTTOM_BAR_H3_LANDSCAPE       160
#define FAN_BOTTOM_BAR_Y2_LANDSCAPE       160
#define FAN_BOTTOM_BAR_H2_LANDSCAPE       105
#define FAN_BOTTOM_BAR_Y1_LANDSCAPE       188
#define FAN_BOTTOM_BAR_H1_LANDSCAPE       50
#define FAN_BOTTOM_ITEM_RADIUS_LANDSCAPE  12
#define FAN_BOTTOM_ITEM_PAD_VER_LANDSCAPE 4
#define FAN_BOTTOM_ITEM_PAD_HOR_LANDSCAPE 10

/* ---------------------------------------------------------------------------
 * Fan supported feature bits (from HA core).
 * ------------------------------------------------------------------------- */
#define FAN_FEATURE_SET_SPEED     (1U << 0)
#define FAN_FEATURE_OSCILLATE     (1U << 1)
#define FAN_FEATURE_DIRECTION     (1U << 2)
#define FAN_FEATURE_PRESET_MODE   (1U << 3)

/* ---------------------------------------------------------------------------
 * Page state kept across rebuilds.
 * ------------------------------------------------------------------------- */
typedef struct {
    int last_percentage;
    bool last_percentage_initialized;
    bool last_oscillating;
    bool last_oscillating_initialized;
} fan_more_info_session_t;

typedef enum {
    FAN_BOTTOM_IDX_PRESET = 0,
    FAN_BOTTOM_IDX_DIR,
    FAN_BOTTOM_IDX_OSC,
    FAN_BOTTOM_COUNT
} fan_bottom_idx_t;

typedef enum {
    FAN_MAIN_CONTROL_ICON_SWITCH = 0,
    FAN_MAIN_CONTROL_LABELED_SWITCH,
    FAN_MAIN_CONTROL_SPEED_BUTTONS,
} fan_main_control_kind_t;

typedef struct {
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    bool available;
    bool on;
    bool state_unknown;
    uint32_t supported_features;

    /* Speed */
    bool has_percentage;
    int percentage;
    int speed_count;
    float percentage_step;

    /* Oscillation */
    bool has_oscillation;
    bool oscillating;

    /* Direction */
    bool has_direction;
    bool direction_forward;

    /* Presets */
    bool has_presets;
    int preset_count;
    char presets[12][32];
    int active_preset;

    /* LVGL objects */
    lv_obj_t *state_label;
    lv_obj_t *time_label;
    lv_obj_t *root;
    lv_obj_t *slider_fill;
    lv_obj_t *slider_handle;
    lv_obj_t *slider_tip;
    lv_obj_t *slider_tip_label;
    fan_main_control_kind_t main_control_kind;
    lv_obj_t *main_control_track;
    lv_obj_t *main_control_knob;
    lv_obj_t *main_option_boxes[6];
    lv_obj_t *main_option_icons[6];
    lv_obj_t *main_option_labels[6];
    lv_obj_t *bottom_popup;
    lv_obj_t *bottom_backdrop;
    lv_obj_t *bottom_value_labels[FAN_BOTTOM_COUNT];
    lv_obj_t *bottom_icon_labels[FAN_BOTTOM_COUNT];
    lv_obj_t *bottom_btns[FAN_BOTTOM_COUNT];
    int open_bottom_idx;
    lv_coord_t bottom_bar_x;
    fan_more_info_session_t *session;
    entity_more_info_close_cb_t close;
    void *close_user_data;
    lv_obj_t *speed_controls[12];
    int speed_control_count;
    int last_drag_percentage;
    bool has_last_drag_percentage;
    bool syncing_slider;
    lv_obj_t *osc_toggle;
    lv_obj_t *dir_toggle;
    lv_obj_t *preset_btns[12];
    lv_obj_t *pwr_btn;
    bool initial_visual_setup_done;
} fan_more_info_page_t;

/* ---------------------------------------------------------------------------
 * Session helpers
 * ------------------------------------------------------------------------- */

static void fan_session_delete_cb(lv_event_t *event)
{
    lv_obj_t *screen = lv_event_get_target_obj(event);
    fan_more_info_session_t *session = lv_event_get_user_data(event);
    if (screen && lv_obj_get_user_data(screen) == session) {
        lv_obj_set_user_data(screen, NULL);
    }
    lv_free(session);
}

static fan_more_info_session_t *fan_get_session(lv_obj_t *screen)
{
    if (!screen) {
        return NULL;
    }
    fan_more_info_session_t *session = lv_obj_get_user_data(screen);
    if (session) {
        return session;
    }
    session = lv_calloc(1, sizeof(*session));
    if (!session) {
        return NULL;
    }
    lv_obj_set_user_data(screen, session);
    lv_obj_add_event_cb(screen,
                        fan_session_delete_cb,
                        LV_EVENT_DELETE,
                        session);
    return session;
}

/* ---------------------------------------------------------------------------
 * Service helpers
 * ------------------------------------------------------------------------- */

static void fan_call_service_data(const char *entity_id,
                                  const char *service,
                                  cJSON *service_data)
{
    if (!entity_id || !entity_id[0]) {
        return;
    }
    esp_err_t err = app_ha_call_entity_service_data("fan",
                                                    service,
                                                    entity_id,
                                                    service_data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "fan.%s for %s failed: %s",
                 service,
                 entity_id,
                 esp_err_to_name(err));
    }
}

static void fan_call_toggle(const char *entity_id)
{
    if (!entity_id || !entity_id[0]) {
        return;
    }
    esp_err_t err = app_ha_call_entity_service("fan", "toggle", entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "fan.toggle for %s failed: %s", entity_id, esp_err_to_name(err));
    }
}

/* ---------------------------------------------------------------------------
 * State readers
 * ------------------------------------------------------------------------- */

static bool fan_get_number(ha_entity_store_t *store,
                           const char *entity_id,
                           const char *attr,
                           double *out)
{
    return ha_entity_store_get_attr_number(store, entity_id, attr, out) == ESP_OK;
}

static void fan_read_page_state(const entity_more_info_context_t *context,
                                fan_more_info_page_t *page)
{
    ha_entity_store_t *store = context->store;
    const char *id = context->snapshot.entity_id;

    snprintf(page->entity_id, sizeof(page->entity_id), "%s", id);
    page->available =
        !entity_handler_snapshot_is_unavailable(&context->snapshot);
    page->on = strcmp(context->snapshot.state, "on") == 0;
    page->state_unknown = strcmp(context->snapshot.state, "unknown") == 0;
    page->close = context->close;
    page->close_user_data = context->close_user_data;

    double num = 0;
    page->active_preset = -1;

    /* Supported features */
    if (fan_get_number(store, id, "supported_features", &num)) {
        page->supported_features = (uint32_t)num;
    }

    /* Percentage */
    if (fan_get_number(store, id, "percentage", &num)) {
        page->has_percentage = true;
        page->percentage = (int)LV_CLAMP(0, (int)(num + 0.5), 100);
    }
    else if (page->session && page->session->last_percentage_initialized) {
        page->has_percentage = true;
        page->percentage = page->session->last_percentage;
    }

    /* Percentage step */
    if (fan_get_number(store, id, "percentage_step", &num)) {
        page->percentage_step = (float)num;
    }
    else {
        page->percentage_step = 1.0f;
    }

    /* Speed count */
    if (fan_get_number(store, id, "speed_count", &num)) {
        page->speed_count = (int)num;
    }
    if (page->speed_count <= 0) {
        page->speed_count = 1;
    }

    /* Oscillation */
    bool osc = false;
    page->has_oscillation =
        (page->supported_features & FAN_FEATURE_OSCILLATE) != 0;
    if (page->has_oscillation) {
        if (ha_entity_store_get_attr_bool(store, id, "oscillating", &osc) == ESP_OK) {
            page->oscillating = osc;
        }
        else if (page->session && page->session->last_oscillating_initialized) {
            page->oscillating = page->session->last_oscillating;
        }
    }

    /* Direction */
    page->has_direction =
        (page->supported_features & FAN_FEATURE_DIRECTION) != 0;
    if (page->has_direction) {
        char dir[16] = {0};
        if (ha_entity_store_get_attr_string(store, id, "direction", dir, sizeof(dir)) == ESP_OK) {
            page->direction_forward = (strcmp(dir, "forward") == 0);
        }
    }

    /* Presets */
    page->has_presets =
        (page->supported_features & FAN_FEATURE_PRESET_MODE) != 0;
    if (page->has_presets) {
        cJSON *modes = NULL;
        if (ha_entity_store_dup_attr_json(store, id, "preset_modes", &modes) == ESP_OK &&
            cJSON_IsArray(modes)) {
            page->preset_count = 0;
            const cJSON *item = NULL;
            cJSON_ArrayForEach(item, modes) {
                if (page->preset_count >= 12 || !cJSON_IsString(item)) {
                    continue;
                }
                i18n_copy(page->presets[page->preset_count],
                          sizeof(page->presets[0]), item->valuestring);
                page->preset_count++;
            }
        }
        cJSON_Delete(modes);

        char current[32] = {0};
        if (ha_entity_store_get_attr_string(store, id, "preset_mode", current, sizeof(current)) == ESP_OK) {
            page->active_preset = -1;
            for (int i = 0; i < page->preset_count; i++) {
                char display_preset[sizeof(page->presets[0])];
                i18n_display_copy(display_preset, sizeof(display_preset), page->presets[i]);
                if (strcmp(current, display_preset) == 0) {
                    page->active_preset = i;
                    break;
                }
            }
        }
    }

    /* Preserve session values after state read. */
    if (page->session) {
        if (page->has_percentage && page->on && page->percentage > 0) {
            page->session->last_percentage = page->percentage;
            page->session->last_percentage_initialized = true;
        }
        if (page->has_oscillation) {
            page->session->last_oscillating = page->oscillating;
            page->session->last_oscillating_initialized = true;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Page deletion callback
 * ------------------------------------------------------------------------- */

static fan_more_info_page_t *s_active_fan_page;

static void fan_page_delete_cb(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    if (page) {
        if (s_active_fan_page == page) {
            s_active_fan_page = NULL;
        }
        lv_free(page);
    }
}

/* ---------------------------------------------------------------------------
 * Tile rendering
 * ------------------------------------------------------------------------- */

static esp_err_t fan_render_tile(ha_entity_store_t *store,
                                 const ha_entity_snapshot_t *snapshot,
                                 entity_display_model_t *model)
{
    (void)store;
    if (!snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    entity_handler_apply_friendly_name(snapshot, model);
    model->background_color = lv_color_hex(FAN_TILE_BG);
    model->title_color = lv_color_hex(FAN_TILE_TITLE);
    model->state_color = lv_color_hex(FAN_TILE_STATE);

    if (entity_handler_snapshot_is_unavailable(snapshot)) {
        model->icon = MDI_SYMBOL_FAN;
        model->icon_color = lv_color_hex(FAN_TILE_DISABLED_ICON);
        model->icon_circle_background_color =
            lv_color_hex(FAN_TILE_DISABLED_CIRCLE);
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        return ESP_OK;
    }

    const bool is_on = strcmp(snapshot->state, "on") == 0;
    model->icon = is_on ? MDI_SYMBOL_FAN : MDI_SYMBOL_FAN_OFF;
    model->icon_color =
        lv_color_hex(is_on ? FAN_TILE_ICON_ON : FAN_TILE_ICON_OFF);
    model->icon_circle_background_color =
        lv_color_hex(is_on ? FAN_TILE_CIRCLE_ON : FAN_TILE_CIRCLE_OFF);

    /* Show percentage in state text if available. */
    double pct = 0;
    if (
        ha_entity_store_get_attr_number(store,
                                        snapshot->entity_id,
                                        "percentage",
                                        &pct) == ESP_OK) {
        snprintf(model->state_text,
                 sizeof(model->state_text),
                 "%.0f%%",
                 pct);
        return ESP_OK;
    }

    // entity_handler_copy_snapshot_state(model->state_text,
    //                                    sizeof(model->state_text),
    //                                    snapshot);
    return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * Action handler
 * ------------------------------------------------------------------------- */

static bool fan_handle_action(const entity_action_context_t *context)
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
    fan_call_toggle(context->entity_id);
    return true;
}

/* ---------------------------------------------------------------------------
 * More Info page: close callback
 * ------------------------------------------------------------------------- */

static void fan_more_info_close_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) {
        context->close(context->close_user_data);
    }
}

/* ---------------------------------------------------------------------------
 * More Info page: layout builders
 * ------------------------------------------------------------------------- */

static lv_obj_t *fan_create_container(lv_obj_t *parent,
                                      int x, int y,
                                      int w, int h)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_pad_all(c, 0, 0);
    return c;
}

static void fan_build_header(lv_obj_t *screen,
                             lv_obj_t *overlay,
                             const entity_more_info_context_t *context)
{
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(header, 0, 0);
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

    lv_obj_t *close_btn = lv_image_create(header);
    lv_img_set_src(close_btn, &close_50);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_btn,
                        fan_more_info_close_event_cb,
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
    lv_obj_set_height(title,
                      lv_font_get_line_height(&ha_font_misan_medium_20));

    lv_obj_t *close_hitbox = lv_obj_create(overlay);
    lv_obj_remove_style_all(close_hitbox);
    lv_obj_remove_flag(close_hitbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(close_hitbox, 50, 50);
    lv_obj_align(close_hitbox, LV_ALIGN_TOP_LEFT, 20, 12);
    lv_obj_add_event_cb(close_hitbox,
                        fan_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);
}

static void fan_build_state_block(lv_obj_t *parent,
                                  const entity_more_info_context_t *context,
                                  fan_more_info_page_t *page)
{
    const bool landscape = bsp_lcd_is_landscape();
    const int block_w = landscape ? FAN_STATE_BLOCK_W_LANDSCAPE
                                  : FAN_DETAIL_CONTENT_WIDTH;
    const int block_x = landscape ? FAN_STATE_BLOCK_X_LANDSCAPE : 20;
    const int block_y = landscape ? FAN_STATE_BLOCK_Y_LANDSCAPE : 82;

    lv_obj_t *block = fan_create_container(parent, 0, 0,
                                           block_w,
                                           LV_SIZE_CONTENT);
    lv_obj_set_pos(block, block_x, block_y);
    lv_obj_set_layout(block, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(block,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(block, 2, 0);

    /* State label */
    char state_text[64] = {0};
    if (!page->available) {
        snprintf(state_text, sizeof(state_text), "%s", i18n_get(I18N_STATE_UNAVAILABLE));
    }
    else if (page->state_unknown) {
        snprintf(state_text, sizeof(state_text), "%s", i18n_get(I18N_STATE_UNKNOWN));
    }
    else if (!page->on) {
        snprintf(state_text, sizeof(state_text), "%s", i18n_get(I18N_STATE_OFF));
    }
    else if (page->has_percentage) {
        snprintf(state_text, sizeof(state_text), "%d%%", page->percentage);
    }
    else {
        snprintf(state_text, sizeof(state_text), "%s", i18n_get(I18N_STATE_ON));
    }

    lv_obj_t *state_label = lv_label_create(block);
    ui_i18n_label_set_text(state_label, state_text);
    lv_obj_set_style_text_font(state_label, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(state_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(state_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(state_label,
                    block_w,
                    lv_font_get_line_height(&ha_font_misan_medium_20));
    page->state_label = state_label;

    /* Relative time */
    char time_text[48] = {0};
    time_t changed_epoch = 0;
    const bool has_time =
        context->snapshot.last_changed[0] &&
        ha_datetime_format_relative_now(context->snapshot.last_changed,
                                        time_text,
                                        sizeof(time_text)) == ESP_OK &&
        ha_datetime_parse_epoch(context->snapshot.last_changed,
                                &changed_epoch) == ESP_OK;
    if (!has_time) {
        time_text[0] = '\0';
    }

    lv_obj_t *time_label = lv_label_create(block);
    ui_i18n_label_set_text(time_label, time_text);
    lv_obj_set_style_text_font(time_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(time_label,
                    block_w,
                    lv_font_get_line_height(&ha_font_misan_regular_16));
    page->time_label = time_label;
    if (time_text[0]) {
        ha_relative_time_label_bind(time_label, changed_epoch);
    }
    else {
        lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void fan_update_slider_bar(fan_more_info_page_t *page)
{
    if (!page || !page->slider_fill || !page->slider_handle) {
        return;
    }

    const bool landscape = bsp_lcd_is_landscape();

    lv_obj_t *track = lv_obj_get_parent(page->slider_fill);
    if (track) {
        lv_obj_set_style_bg_color(track, lv_color_hex(
            page->on ? FAN_VSLIDER_COLOR : 0x999999), 0);
    }

    const int pct = LV_CLAMP(0, page->percentage, 100);

    if (landscape) {
        const int fill_height = (FAN_HSLIDER_H_LANDSCAPE * pct + 99) / 100;
        const int fill_y = FAN_HSLIDER_H_LANDSCAPE - fill_height;

        if (fill_height <= 0) {
            lv_obj_add_flag(page->slider_fill, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(page->slider_fill, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(page->slider_fill, 0, fill_y);
            lv_obj_set_size(page->slider_fill, FAN_HSLIDER_W_LANDSCAPE,
                            fill_height);
        }

        lv_obj_remove_flag(page->slider_handle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(page->slider_handle, LV_OPA_COVER, 0);
        lv_obj_set_pos(page->slider_handle,
                       (FAN_HSLIDER_W_LANDSCAPE -
                        FAN_HSLIDER_INDICATOR_W_LANDSCAPE) / 2,
                       fill_y + FAN_HSLIDER_INDICATOR_TOP_LANDSCAPE);

        if (page->slider_tip) {
            int tip_y = fill_y - FAN_HSLIDER_TIP_H_LANDSCAPE / 2;
            tip_y = LV_CLAMP(0, tip_y,
                             FAN_HSLIDER_H_LANDSCAPE -
                                 FAN_HSLIDER_TIP_H_LANDSCAPE);
            lv_obj_set_pos(page->slider_tip,
                           FAN_HSLIDER_X_LANDSCAPE -
                               FAN_HSLIDER_TIP_W_LANDSCAPE - 4,
                           FAN_HSLIDER_Y_LANDSCAPE + tip_y);
        }
    } else {
        const int fill_height = (FAN_VSLIDER_HEIGHT * pct + 99) / 100;
        const int fill_y = FAN_VSLIDER_HEIGHT - fill_height;

        if (fill_height <= 0) {
            lv_obj_add_flag(page->slider_fill, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(page->slider_fill, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(page->slider_fill, 0, fill_y);
            lv_obj_set_size(page->slider_fill, FAN_VSLIDER_WIDTH, fill_height);
        }

        lv_obj_remove_flag(page->slider_handle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(page->slider_handle, LV_OPA_COVER, 0);
        lv_obj_set_pos(page->slider_handle,
                       (FAN_VSLIDER_WIDTH - FAN_VSLIDER_INDICATOR_W) / 2,
                       fill_y + FAN_VSLIDER_INDICATOR_TOP);

        if (page->slider_tip) {
            int tip_y = fill_y - FAN_VSLIDER_TIP_H / 2;
            tip_y = LV_CLAMP(0, tip_y,
                             FAN_VSLIDER_HEIGHT - FAN_VSLIDER_TIP_H);
            lv_obj_set_pos(page->slider_tip, FAN_VSLIDER_X - 64,
                           FAN_VSLIDER_Y + tip_y);
        }
    }
}

static void fan_update_slider_visual(fan_more_info_page_t *page)
{
    fan_update_slider_bar(page);
    if (!page || !page->slider_fill || !page->slider_handle) {
        return;
    }
    const int pct = LV_CLAMP(0, page->percentage, 100);
    if (page->slider_tip && page->slider_tip_label) {
        char text[8];
        snprintf(text, sizeof(text), "%d%%", pct);
        lv_label_set_text(page->slider_tip_label, text);
    }
    if (page->state_label) {
        char text[16];
        if (!page->available) {
            snprintf(text, sizeof(text), "%s", i18n_get(I18N_STATE_UNAVAILABLE));
        } else if (page->state_unknown) {
            snprintf(text, sizeof(text), "%s", i18n_get(I18N_STATE_UNKNOWN));
        } else {
            if (pct > 0) {
                snprintf(text, sizeof(text), "%d%%", pct);
            } else {
                snprintf(text, sizeof(text), "%s", i18n_get(I18N_STATE_OFF));
            }
        }
        lv_label_set_text(page->state_label, text);
    }
}

static void fan_slider_anim_cb(void *var, int32_t v)
{
    fan_more_info_page_t *page = (fan_more_info_page_t *)var;
    page->percentage = v;
    fan_update_slider_bar(page);
    if (page->speed_control_count > 0 && page->speed_controls[0]) {
        page->syncing_slider = true;
        lv_slider_set_value(page->speed_controls[0], v, LV_ANIM_OFF);
        page->syncing_slider = false;
    }
}

static void fan_slider_anim_ready_cb(lv_anim_t *a)
{
    fan_more_info_page_t *page = (fan_more_info_page_t *)a->var;
    if (!page) return;
    fan_update_slider_visual(page);
}

static void fan_animate_slider(fan_more_info_page_t *page, int to_pct)
{
    if (!page->slider_fill || !page->slider_handle) {
        page->percentage = to_pct;
        fan_update_slider_visual(page);
        return;
    }
    int from_pct = page->percentage;
    if (from_pct == to_pct) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, page);
    lv_anim_set_exec_cb(&a, fan_slider_anim_cb);
    lv_anim_set_values(&a, from_pct, to_pct);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&a, fan_slider_anim_ready_cb);
    lv_anim_start(&a);
}

static void fan_icon_switch_knob_anim_cb(void *var, int32_t v)
{
    lv_obj_t *knob = (lv_obj_t *)var;
    lv_obj_set_y(knob, v);
}

static int fan_speed_buttons_selected(const fan_more_info_page_t *page,
                                       int option_count)
{
    if (!page->on || page->percentage <= 0) {
        return option_count - 1;
    }
    const int counted = option_count - 1;
    int selected = counted - (page->percentage * counted + 99) / 100;
    if (selected < 0) selected = 0;
    if (selected >= counted) selected = counted - 1;
    return selected;
}

static void fan_update_state_label(fan_more_info_page_t *page)
{
    if (!page || !page->state_label) {
        return;
    }
    char text[16];
    if (!page->available) {
        snprintf(text, sizeof(text), "%s", i18n_get(I18N_STATE_UNAVAILABLE));
    }
    else if (page->state_unknown) {
        snprintf(text, sizeof(text), "%s", i18n_get(I18N_STATE_UNKNOWN));
    }
    else if (!page->on) {
        snprintf(text, sizeof(text), "%s", i18n_get(I18N_STATE_OFF));
    }
    else if (page->has_percentage) {
        snprintf(text, sizeof(text), "%d%%", page->percentage);
    }
    else {
        snprintf(text, sizeof(text), "%s", i18n_get(I18N_STATE_ON));
    }
    lv_label_set_text(page->state_label, text);
}

static void fan_update_main_control_visual(fan_more_info_page_t *page)
{
    if (!page || !page->main_control_track) {
        return;
    }

    const uint32_t track_color = page->on
                                     ? FAN_CONTROL_ON_COLOR
                                     : FAN_CONTROL_OFF_COLOR;
    lv_obj_set_style_bg_color(page->main_control_track,
                              lv_color_hex(track_color), 0);
    lv_obj_set_style_bg_opa(page->main_control_track,
                            page->on ? LV_OPA_30 : LV_OPA_20, 0);
    lv_obj_set_style_opa(page->main_control_track,
                         page->available ? LV_OPA_COVER : LV_OPA_40, 0);

    if (page->main_control_kind == FAN_MAIN_CONTROL_ICON_SWITCH) {
        const bool landscape = bsp_lcd_is_landscape();
        const int knob_y = page->on
                               ? FAN_SWITCH_INSET
                               : (landscape
                                      ? FAN_SWITCH_H_LANDSCAPE
                                      : FAN_SWITCH_H) -
                                     FAN_SWITCH_INSET -
                                     (landscape ? FAN_SWITCH_KNOB_H_LANDSCAPE
                                                : FAN_SWITCH_KNOB_H);
        lv_obj_set_style_bg_color(page->main_control_knob,
                                  lv_color_hex(page->on
                                                   ? FAN_CONTROL_ON_COLOR
                                                   : FAN_CONTROL_OFF_COLOR),
                                  0);
        lv_obj_set_style_bg_opa(page->main_control_knob, LV_OPA_COVER, 0);
        if (page->main_option_icons[0]) {
            lv_label_set_text(page->main_option_icons[0],
                              page->on ? MDI_SYMBOL_FAN
                                       : MDI_SYMBOL_FAN_OFF);
        }
        if (page->initial_visual_setup_done) {
            lv_anim_del(page->main_control_knob, fan_icon_switch_knob_anim_cb);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, page->main_control_knob);
            lv_anim_set_exec_cb(&a, fan_icon_switch_knob_anim_cb);
            lv_anim_set_values(&a, lv_obj_get_y(page->main_control_knob), knob_y);
            lv_anim_set_time(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
        } else {
            lv_obj_set_y(page->main_control_knob, knob_y);
        }
        return;
    }

    const int speed_option_count = page->speed_control_count;
    const int selected =
        page->main_control_kind == FAN_MAIN_CONTROL_SPEED_BUTTONS
            ? fan_speed_buttons_selected(page, speed_option_count)
            : (page->on ? 0 : 1);
    const int option_count =
        page->main_control_kind == FAN_MAIN_CONTROL_SPEED_BUTTONS
            ? speed_option_count : 2;

    for (int i = 0; i < option_count; i++) {
        const bool active = i == selected;
        lv_obj_set_style_bg_opa(page->main_option_boxes[i],
                                active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(page->main_option_boxes[i],
                                  lv_color_hex((i == option_count - 1)
                                                   ? FAN_CONTROL_OFF_COLOR
                                                   : FAN_CONTROL_ON_COLOR),
                                  0);
        if (page->main_option_icons[i]) {
            lv_obj_set_style_text_color(page->main_option_icons[i],
                                        lv_color_hex(active
                                                         ? 0xFFFFFF
                                                         : FAN_CONTROL_INACTIVE_FG),
                                        0);
        }
        if (page->main_option_labels[i]) {
            lv_obj_set_style_text_color(page->main_option_labels[i],
                                        lv_color_hex(active
                                                         ? 0xFFFFFF
                                                         : FAN_CONTROL_INACTIVE_FG),
                                        0);
        }
    }
}

static void fan_close_bottom_popup_v2(fan_more_info_page_t *page)
{
    if (!page || !page->bottom_popup) {
        return;
    }
    if (page->open_bottom_idx >= 0 && page->bottom_btns[page->open_bottom_idx]) {
        lv_obj_set_style_bg_color(page->bottom_btns[page->open_bottom_idx],
                                  lv_color_hex(FAN_BOTTOM_BG_DEFAULT), 0);
    }
    lv_obj_t *popup = page->bottom_popup;
    page->bottom_popup = NULL;
    page->open_bottom_idx = -1;
    lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(popup);
    if (page->bottom_backdrop) {
        lv_obj_delete_async(page->bottom_backdrop);
        page->bottom_backdrop = NULL;
    }
}

static esp_err_t fan_more_info_update(
    const entity_more_info_context_t *context)
{
    fan_more_info_page_t *page = s_active_fan_page;
    if (!context || !page || !page->root ||
        lv_obj_get_parent(page->root) != context->screen ||
        strcmp(page->entity_id, context->snapshot.entity_id) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    fan_read_page_state(context, page);
    const int target_pct = page->on ? page->percentage : 0;

    fan_update_state_label(page);
    if (page->main_control_track) {
        fan_update_main_control_visual(page);
        page->initial_visual_setup_done = true;
    }
    else if (page->slider_fill && page->slider_handle) {
        page->percentage = target_pct;
        fan_update_slider_visual(page);
    }

    if (page->bottom_value_labels[FAN_BOTTOM_IDX_DIR]) {
        lv_label_set_text(page->bottom_value_labels[FAN_BOTTOM_IDX_DIR],
                          page->direction_forward
                              ? i18n_get(I18N_FAN_FORWARD)
                              : i18n_get(I18N_FAN_REVERSE));
    }
    if (page->bottom_icon_labels[FAN_BOTTOM_IDX_DIR]) {
        lv_label_set_text(page->bottom_icon_labels[FAN_BOTTOM_IDX_DIR],
                          page->direction_forward
                              ? MDI_SYMBOL_ROTATE_RIGHT
                              : MDI_SYMBOL_ROTATE_LEFT);
    }

    if (page->bottom_value_labels[FAN_BOTTOM_IDX_OSC]) {
        lv_label_set_text(page->bottom_value_labels[FAN_BOTTOM_IDX_OSC],
                          page->oscillating
                              ? i18n_get(I18N_STATE_ON)
                              : i18n_get(I18N_STATE_OFF));
    }
    if (page->bottom_icon_labels[FAN_BOTTOM_IDX_OSC]) {
        lv_label_set_text(page->bottom_icon_labels[FAN_BOTTOM_IDX_OSC],
                          page->oscillating
                              ? MDI_SYMBOL_ARROW_OSCILLATING
                              : MDI_SYMBOL_ARROW_OSCILLATING_OFF);
    }

    fan_close_bottom_popup_v2(page);

    if (page->time_label) {
        time_t changed_epoch = 0;
        if (context->snapshot.last_changed[0] &&
            ha_datetime_parse_epoch(context->snapshot.last_changed,
                                    &changed_epoch) == ESP_OK) {
            ha_relative_time_label_bind(page->time_label, changed_epoch);
            lv_obj_remove_flag(page->time_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (page->slider_fill) {
        lv_obj_t *track = lv_obj_get_parent(page->slider_fill);
        if (track) {
            lv_obj_set_style_opa(track,
                                 page->available ? LV_OPA_COVER : LV_OPA_40,
                                 0);
        }
    }

    for (int i = 0; i < page->speed_control_count; i++) {
        if (!page->speed_controls[i]) {
            continue;
        }
        if (page->available) {
            lv_obj_remove_state(page->speed_controls[i], LV_STATE_DISABLED);
        }
        else {
            lv_obj_add_state(page->speed_controls[i], LV_STATE_DISABLED);
        }
    }
    if (page->pwr_btn) {
        if (page->available) {
            lv_obj_remove_state(page->pwr_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(page->pwr_btn, LV_STATE_DISABLED);
        }
        lv_obj_set_style_opa(page->pwr_btn,
                             page->available ? LV_OPA_COVER : LV_OPA_40, 0);
    }

    for (int i = 0; i < FAN_BOTTOM_COUNT; i++) {
        if (!page->bottom_btns[i]) continue;
        bool item_enabled;
        if (i == FAN_BOTTOM_IDX_PRESET) {
            item_enabled = page->has_presets && page->preset_count > 0;
        } else if (i == FAN_BOTTOM_IDX_DIR) {
            item_enabled = page->has_direction;
        } else {
            item_enabled = page->has_oscillation;
        }
        lv_obj_set_style_opa(page->bottom_btns[i],
                             (page->available && item_enabled)
                                 ? LV_OPA_COVER : LV_OPA_40, 0);
    }

    return ESP_OK;
}

static void fan_commit_percentage(fan_more_info_page_t *page)
{
    if (!page || !page->available) {
        return;
    }
    page->percentage = LV_CLAMP(0, page->percentage, 100);
    page->on = page->percentage > 0;
    if (page->session && page->percentage > 0) {
        page->session->last_percentage = page->percentage;
        page->session->last_percentage_initialized = true;
    }

    if (!page->on) {
        fan_call_service_data(page->entity_id, "turn_off", NULL);
        return;
    }
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return;
    }
    cJSON_AddNumberToObject(data, "percentage", page->percentage);
    fan_call_service_data(page->entity_id, "set_percentage", data);
    cJSON_Delete(data);
}

static void fan_set_power(fan_more_info_page_t *page, bool turn_on)
{
    if (!page || !page->available) {
        return;
    }
    fan_call_service_data(page->entity_id,
                          turn_on ? "turn_on" : "turn_off",
                          NULL);
    page->on = turn_on;
    if (turn_on && page->has_percentage && page->percentage <= 0) {
        if (page->session && page->session->last_percentage_initialized) {
            page->percentage = page->session->last_percentage;
        }
        else {
            page->percentage = 100;
        }
    }
    fan_update_state_label(page);
    fan_update_main_control_visual(page);
}

static void fan_icon_switch_event_cb(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && page) {
        fan_set_power(page, !page->on);
    }
}

static void fan_labeled_switch_event_cb(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    lv_obj_t *option = lv_event_get_current_target_obj(event);
    if (!page || !option || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    fan_set_power(page, (int)(uintptr_t)lv_obj_get_user_data(option) == 0);
}

static void fan_speed_buttons_event_cb(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    lv_obj_t *option = lv_event_get_current_target_obj(event);
    if (!page || !option || !page->available ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const int option_count = page->speed_control_count;
    const int index = (int)(uintptr_t)lv_obj_get_user_data(option);
    if (index < 0 || index >= option_count) {
        return;
    }
    const int counted = option_count - 1;
    page->percentage = (index == option_count - 1)
        ? 0
        : ((counted - index) * 100) / counted;
    page->on = page->percentage > 0;
    fan_commit_percentage(page);
    fan_update_state_label(page);
    fan_update_main_control_visual(page);
}

static void fan_create_option_content(fan_more_info_page_t *page,
                                      lv_obj_t *option,
                                      int index,
                                      const char *icon_text,
                                      const char *label_text,
                                      int content_width,
                                      int icon_y,
                                      int label_y,
                                      bool horizontal)
{
    if (horizontal) {
        lv_obj_set_flex_flow(option, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(option,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(option, 4, 0);
    }

    lv_obj_t *icon = lv_label_create(option);
    lv_label_set_text(icon, icon_text);
    lv_obj_set_style_text_font(icon,
                               &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(FAN_CONTROL_INACTIVE_FG), 0);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
    if (horizontal) {
        lv_obj_set_width(icon, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_pos(icon, 0, icon_y);
        lv_obj_set_size(icon, content_width, 24);
    }
    page->main_option_icons[index] = icon;

    if (label_text) {
        lv_obj_t *label = lv_label_create(option);
        lv_label_set_text(label, label_text);
        lv_obj_set_style_text_font(label, &ha_font_misan_regular_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(FAN_CONTROL_INACTIVE_FG), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        if (horizontal) {
            lv_obj_set_width(label, LV_SIZE_CONTENT);
        } else {
            lv_obj_set_pos(label, 0, label_y);
            lv_obj_set_size(label, content_width, 20);
        }
        page->main_option_labels[index] = label;
    }
}

static lv_obj_t *fan_create_main_track(lv_obj_t *parent,
                                        int x, int y,
                                        int width, int height);

static void fan_build_speed_buttons(lv_obj_t *parent,
                                    fan_more_info_page_t *page)
{
    page->main_control_kind = FAN_MAIN_CONTROL_SPEED_BUTTONS;
    const int option_count = page->speed_count + 1;
    if (option_count < 2) return;

    const bool landscape = bsp_lcd_is_landscape();
    const bool is_on_off_switch = option_count == 2;
    const int max_track_h = FAN_BOTTOM_BAR_Y - FAN_SPEED_SWITCH_Y;
    int option_h, track_h;
    int option_w, track_w;
    int track_x, track_y;

    if (landscape) {
        if (is_on_off_switch) {
            option_h = FAN_SWITCH_KNOB_H_LANDSCAPE;
            track_h = FAN_SWITCH_H_LANDSCAPE;
        } else {
            option_h = FAN_SPEED_OPTION_H_LANDSCAPE;
            track_h = FAN_SPEED_SWITCH_H_LANDSCAPE;
        }

        lv_obj_t *track = fan_create_main_track(parent,
                                                is_on_off_switch
                                                    ? FAN_SWITCH_X_LANDSCAPE
                                                    : FAN_SPEED_SWITCH_X_LANDSCAPE,
                                                FAN_SPEED_SWITCH_Y_LANDSCAPE,
                                                is_on_off_switch
                                                    ? FAN_SWITCH_W_LANDSCAPE
                                                    : FAN_SPEED_SWITCH_W_LANDSCAPE,
                                                track_h);
        page->main_control_track = track;
        lv_obj_set_style_radius(track, FAN_SWITCH_RADIUS_LANDSCAPE, 0);
    } else {
        if (is_on_off_switch) {
            option_h = FAN_SWITCH_KNOB_H;
            track_h = FAN_SWITCH_H;
        } else {
            option_h = (max_track_h - 2 * FAN_SWITCH_INSET) / option_count;
            if (option_h > FAN_SPEED_OPTION_H) option_h = FAN_SPEED_OPTION_H;
            if (option_h < 38) option_h = 38;
            track_h = 2 * FAN_SWITCH_INSET + option_count * option_h;
        }

        lv_obj_t *track = fan_create_main_track(parent,
                                                is_on_off_switch
                                                    ? FAN_SWITCH_X
                                                    : FAN_SPEED_SWITCH_X,
                                                FAN_SPEED_SWITCH_Y,
                                                is_on_off_switch
                                                    ? FAN_SWITCH_W
                                                    : FAN_SPEED_SWITCH_W,
                                                track_h);
        page->main_control_track = track;
    }

    static const char *icons_2[]  = {MDI_SYMBOL_FAN, MDI_SYMBOL_FAN_OFF};
    const char *labels_2[] = {i18n_get(I18N_STATE_ON), i18n_get(I18N_STATE_OFF)};
    static const char *icons_3[]  = {MDI_SYMBOL_FAN_SPEED_2,
                                     MDI_SYMBOL_FAN_SPEED_1,
                                     MDI_SYMBOL_FAN_OFF};
    const char *labels_3[] = {i18n_get(I18N_OPTION_FAN_HIGH),
                               i18n_get(I18N_OPTION_FAN_LOW),
                               i18n_get(I18N_STATE_OFF)};
    static const char *icons_4[]  = {MDI_SYMBOL_FAN_SPEED_3,
                                     MDI_SYMBOL_FAN_SPEED_2,
                                     MDI_SYMBOL_FAN_SPEED_1,
                                     MDI_SYMBOL_FAN_OFF};
    const char *labels_4[] = {i18n_get(I18N_OPTION_FAN_HIGH),
                               i18n_get(I18N_OPTION_FAN_MEDIUM),
                               i18n_get(I18N_OPTION_FAN_LOW),
                               i18n_get(I18N_STATE_OFF)};
    static const char *icons_5[]  = {MDI_SYMBOL_FAN_SPEED_3,
                                     MDI_SYMBOL_FAN_SPEED_2,
                                     MDI_SYMBOL_FAN_SPEED_1,
                                     MDI_SYMBOL_FAN,
                                     MDI_SYMBOL_FAN_OFF};
    const char *labels_5[] = {"4", "3", "2", "1", i18n_get(I18N_STATE_OFF)};
    static const char *icons_6[]  = {MDI_SYMBOL_FAN_SPEED_3,
                                     MDI_SYMBOL_FAN_SPEED_2,
                                     MDI_SYMBOL_FAN_SPEED_1,
                                     MDI_SYMBOL_FAN,
                                     MDI_SYMBOL_FAN,
                                     MDI_SYMBOL_FAN_OFF};
    const char *labels_6[] = {"5", "4", "3", "2", "1", i18n_get(I18N_STATE_OFF)};

    const char **icons = NULL;
    const char **labels = NULL;
    switch (option_count) {
    case 2: icons = icons_2; labels = labels_2; break;
    case 3: icons = icons_3; labels = labels_3; break;
    case 4: icons = icons_4; labels = labels_4; break;
    case 5: icons = icons_5; labels = labels_5; break;
    default: icons = icons_6; labels = labels_6; break;
    }

    const int icon_y = is_on_off_switch ? 30 : option_h / 7;
    const int label_y = is_on_off_switch ? 58 : option_h / 2;

    for (int i = 0; i < option_count; i++) {
        lv_obj_t *option = fan_create_container(
            page->main_control_track,
            FAN_SWITCH_INSET,
            FAN_SWITCH_INSET + i * option_h,
            landscape ? (is_on_off_switch
                             ? FAN_SWITCH_KNOB_W_LANDSCAPE
                             : FAN_SPEED_OPTION_W_LANDSCAPE)
                      : FAN_SPEED_OPTION_W,
            option_h);
        page->main_option_boxes[i] = option;
        lv_obj_set_style_radius(option,
                                landscape ? 12
                                          : FAN_SWITCH_RADIUS, 0);
        lv_obj_add_flag(option, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(option, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(option, fan_speed_buttons_event_cb,
                            LV_EVENT_CLICKED, page);
        fan_create_option_content(page, option, i, icons[i], labels[i],
                                  landscape
                                      ? (is_on_off_switch
                                             ? FAN_SWITCH_KNOB_W_LANDSCAPE
                                             : FAN_SPEED_OPTION_W_LANDSCAPE)
                                      : FAN_SPEED_OPTION_W,
                                  icon_y, label_y,
                                  landscape && !is_on_off_switch);
        page->speed_controls[i] = option;
    }
    page->speed_control_count = option_count;
    fan_update_main_control_visual(page);
}

static lv_obj_t *fan_create_main_track(lv_obj_t *parent,
                                        int x, int y,
                                        int width, int height)
{
    lv_obj_t *track = fan_create_container(parent, x, y, width, height);
    lv_obj_set_style_radius(track, FAN_SWITCH_RADIUS, 0);
    lv_obj_set_style_clip_corner(track, true, 0);
    return track;
}


static void fan_build_icon_switch(lv_obj_t *parent,
                                  fan_more_info_page_t *page)
{
    const bool landscape = bsp_lcd_is_landscape();
    page->main_control_kind = FAN_MAIN_CONTROL_ICON_SWITCH;

    if (landscape) {
        lv_obj_t *track = fan_create_main_track(parent,
                                                FAN_SWITCH_X_LANDSCAPE,
                                                FAN_SWITCH_Y_LANDSCAPE,
                                                FAN_SWITCH_W_LANDSCAPE,
                                                FAN_SWITCH_H_LANDSCAPE);
        page->main_control_track = track;
        lv_obj_set_style_radius(track, FAN_SWITCH_RADIUS_LANDSCAPE, 0);
        lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(track, fan_icon_switch_event_cb,
                            LV_EVENT_CLICKED, page);

        const int knob_w = FAN_SWITCH_W_LANDSCAPE - 2 * FAN_SWITCH_INSET;
        const int knob_h = FAN_SWITCH_KNOB_H_LANDSCAPE;
        lv_obj_t *knob = fan_create_container(track,
                                              FAN_SWITCH_INSET,
                                              FAN_SWITCH_INSET,
                                              knob_w,
                                              knob_h);
        page->main_control_knob = knob;
        lv_obj_set_style_radius(knob, FAN_SWITCH_RADIUS_LANDSCAPE, 0);
        lv_obj_remove_flag(knob, LV_OBJ_FLAG_CLICKABLE);
        fan_create_option_content(page, knob, 0, MDI_SYMBOL_FAN, NULL,
                                  knob_w,
                                  (knob_h - 21) / 2, 0, false);

        page->speed_controls[0] = track;
        page->speed_control_count = 1;
        fan_update_main_control_visual(page);
        return;
    }

    lv_obj_t *track = fan_create_main_track(parent,
                                            FAN_SWITCH_X,
                                            FAN_SWITCH_Y,
                                            FAN_SWITCH_W,
                                            FAN_SWITCH_H);
    page->main_control_track = track;
    lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(track, fan_icon_switch_event_cb,
                        LV_EVENT_CLICKED, page);

    lv_obj_t *knob = fan_create_container(track,
                                          FAN_SWITCH_INSET,
                                          FAN_SWITCH_INSET,
                                          FAN_SWITCH_KNOB_W,
                                          FAN_SWITCH_KNOB_H);
    page->main_control_knob = knob;
    lv_obj_set_style_radius(knob, FAN_SWITCH_RADIUS, 0);
    lv_obj_remove_flag(knob, LV_OBJ_FLAG_CLICKABLE);
    fan_create_option_content(page, knob, 0, MDI_SYMBOL_FAN, NULL,
                              FAN_SWITCH_KNOB_W,
                              (FAN_SWITCH_KNOB_H - 24) / 2, 0, false);

    page->speed_controls[0] = track;
    page->speed_control_count = 1;
    fan_update_main_control_visual(page);
}

static void fan_build_labeled_switch(lv_obj_t *parent,
                                     fan_more_info_page_t *page)
{
    const bool landscape = bsp_lcd_is_landscape();
    page->main_control_kind = FAN_MAIN_CONTROL_LABELED_SWITCH;

    if (landscape) {
        lv_obj_t *track = fan_create_main_track(parent,
                                                FAN_SWITCH_X_LANDSCAPE,
                                                FAN_SWITCH_Y_LANDSCAPE,
                                                FAN_SWITCH_W_LANDSCAPE,
                                                FAN_SWITCH_H_LANDSCAPE);
        page->main_control_track = track;
        lv_obj_set_style_radius(track, FAN_SWITCH_RADIUS_LANDSCAPE, 0);

        static const char *icons[2] = {MDI_SYMBOL_FAN, MDI_SYMBOL_FAN_OFF};
        const char *labels[2] = {i18n_get(I18N_STATE_ON), i18n_get(I18N_STATE_OFF)};
        for (int i = 0; i < 2; i++) {
            lv_obj_t *option = fan_create_container(
                track,
                FAN_SWITCH_INSET,
                FAN_SWITCH_INSET + i * FAN_SWITCH_KNOB_H_LANDSCAPE,
                FAN_SWITCH_KNOB_W_LANDSCAPE, FAN_SWITCH_KNOB_H_LANDSCAPE);
            page->main_option_boxes[i] = option;
            lv_obj_set_style_radius(option, FAN_SWITCH_RADIUS_LANDSCAPE, 0);
            lv_obj_add_flag(option, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_user_data(option, (void *)(uintptr_t)i);
            lv_obj_add_event_cb(option, fan_labeled_switch_event_cb,
                                LV_EVENT_CLICKED, page);
            fan_create_option_content(page, option, i, icons[i], labels[i],
                                      FAN_SWITCH_KNOB_W_LANDSCAPE,
                                      18, 38, false);
            page->speed_controls[i] = option;
        }
        page->speed_control_count = 2;
        fan_update_main_control_visual(page);
        return;
    }

    lv_obj_t *track = fan_create_main_track(parent,
                                            FAN_SWITCH_X,
                                            FAN_SWITCH_Y,
                                            FAN_SWITCH_W,
                                            FAN_SWITCH_H);
    page->main_control_track = track;

    static const char *icons[2] = {MDI_SYMBOL_FAN, MDI_SYMBOL_FAN_OFF};
    const char *labels[2] = {i18n_get(I18N_STATE_ON), i18n_get(I18N_STATE_OFF)};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *option = fan_create_container(
            track,
            FAN_SWITCH_INSET,
            FAN_SWITCH_INSET + i * FAN_SWITCH_KNOB_H,
            FAN_SWITCH_KNOB_W,
            FAN_SWITCH_KNOB_H);
        page->main_option_boxes[i] = option;
        lv_obj_set_style_radius(option, FAN_SWITCH_RADIUS, 0);
        lv_obj_add_flag(option, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(option, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(option, fan_labeled_switch_event_cb,
                            LV_EVENT_CLICKED, page);
        fan_create_option_content(page, option, i, icons[i], labels[i],
                                  FAN_SWITCH_KNOB_W,
                                  30, 58, false);
        page->speed_controls[i] = option;
    }
    page->speed_control_count = 2;
    fan_update_main_control_visual(page);
}

static void fan_vslider_event_cb(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    lv_obj_t *slider = lv_event_get_current_target_obj(event);
    if (!page || !slider || !page->available) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    if (page->syncing_slider) {
        return;
    }
    if (code == LV_EVENT_PRESSED) {
        lv_anim_del(page, fan_slider_anim_cb);
        page->percentage = LV_CLAMP(0, (int)lv_slider_get_value(slider), 100);
        page->last_drag_percentage = page->percentage;
        page->has_last_drag_percentage = false;
        page->on = page->percentage > 0;
        fan_update_slider_visual(page);
    }
    else if (code == LV_EVENT_PRESSING) {
        /* Keep the last stable sample while the finger is still touching the
         * panel. LVGL samples once more during RELEASED, where capacitive
         * touch coordinates can drift upward as the finger leaves the glass. */
        if (lv_slider_is_dragged(slider)) {
            page->last_drag_percentage =
                LV_CLAMP(0, (int)lv_slider_get_value(slider), 100);
            page->has_last_drag_percentage = true;
        }
    }
    else if (code == LV_EVENT_VALUE_CHANGED) {
        page->percentage = LV_CLAMP(0, (int)lv_slider_get_value(slider), 100);
        page->on = page->percentage > 0;
        lv_obj_remove_flag(page->slider_tip, LV_OBJ_FLAG_HIDDEN);
        fan_update_slider_visual(page);
    }
    else if (code == LV_EVENT_RELEASED) {
        if (page->has_last_drag_percentage) {
            page->percentage = page->last_drag_percentage;
            page->syncing_slider = true;
            lv_slider_set_value(slider, page->percentage, LV_ANIM_OFF);
            page->syncing_slider = false;
            fan_update_slider_visual(page);
        }
        else {
            page->percentage =
                LV_CLAMP(0, (int)lv_slider_get_value(slider), 100);
        }
        page->has_last_drag_percentage = false;
        lv_obj_add_flag(page->slider_tip, LV_OBJ_FLAG_HIDDEN);
        fan_commit_percentage(page);
    }
    else if (code == LV_EVENT_PRESS_LOST) {
        page->has_last_drag_percentage = false;
        lv_obj_add_flag(page->slider_tip, LV_OBJ_FLAG_HIDDEN);
    }
}

static void fan_build_vertical_slider(lv_obj_t *parent, fan_more_info_page_t *page)
{
    const bool landscape = bsp_lcd_is_landscape();

    if (landscape) {
        lv_obj_t *track = fan_create_container(parent,
                                               FAN_HSLIDER_X_LANDSCAPE,
                                               FAN_HSLIDER_Y_LANDSCAPE,
                                               FAN_HSLIDER_W_LANDSCAPE,
                                               FAN_HSLIDER_H_LANDSCAPE);
        lv_obj_set_style_bg_color(track, lv_color_hex(
            page->on ? FAN_VSLIDER_COLOR : 0x999999), 0);
        lv_obj_set_style_bg_opa(track, FAN_VSLIDER_BG_OPA, 0);
        lv_obj_set_style_radius(track, FAN_HSLIDER_RADIUS_LANDSCAPE, 0);
        lv_obj_set_style_clip_corner(track, true, 0);

        page->slider_fill = lv_obj_create(track);
        lv_obj_remove_style_all(page->slider_fill);
        lv_obj_remove_flag(page->slider_fill, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(page->slider_fill, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(page->slider_fill,
                                  lv_color_hex(FAN_VSLIDER_COLOR), 0);
        lv_obj_set_style_bg_opa(page->slider_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(page->slider_fill,
                                FAN_HSLIDER_RADIUS_LANDSCAPE, 0);

        page->slider_handle = lv_obj_create(track);
        lv_obj_remove_style_all(page->slider_handle);
        lv_obj_remove_flag(page->slider_handle, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(page->slider_handle, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(page->slider_handle,
                        FAN_HSLIDER_INDICATOR_W_LANDSCAPE,
                        FAN_HSLIDER_INDICATOR_H_LANDSCAPE);
        lv_obj_set_style_bg_color(page->slider_handle,
                                  lv_color_hex(FAN_VSLIDER_INDICATOR_CLR),
                                  0);
        lv_obj_set_style_bg_opa(page->slider_handle, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(page->slider_handle,
                                FAN_HSLIDER_INDICATOR_R_LANDSCAPE, 0);

        page->slider_tip = lv_obj_create(parent);
        lv_obj_remove_style_all(page->slider_tip);
        lv_obj_remove_flag(page->slider_tip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(page->slider_tip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(page->slider_tip,
                        FAN_HSLIDER_TIP_W_LANDSCAPE,
                        FAN_HSLIDER_TIP_H_LANDSCAPE);
        lv_obj_set_style_radius(page->slider_tip, 7, 0);
        lv_obj_set_style_bg_color(page->slider_tip, lv_color_hex(0x242424), 0);
        lv_obj_set_style_bg_opa(page->slider_tip, LV_OPA_COVER, 0);
        page->slider_tip_label = lv_label_create(page->slider_tip);
        lv_obj_set_style_text_font(page->slider_tip_label,
                                   &ha_font_misan_regular_14, 0);
        lv_obj_set_style_text_color(page->slider_tip_label,
                                    lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(page->slider_tip_label);
        lv_obj_add_flag(page->slider_tip, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *slider = lv_slider_create(track);
        lv_obj_set_size(slider,
                        FAN_HSLIDER_W_LANDSCAPE,
                        FAN_HSLIDER_H_LANDSCAPE);
        lv_obj_set_pos(slider, 0, 0);
        lv_slider_set_orientation(slider, LV_SLIDER_ORIENTATION_VERTICAL);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, page->percentage, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_border_width(slider, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(slider, 0, LV_PART_INDICATOR);
        lv_obj_set_style_border_width(slider, 0, LV_PART_KNOB);
        lv_obj_set_style_shadow_width(slider, 0, LV_PART_KNOB);
        lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
        lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(slider, 0, LV_PART_INDICATOR);
        lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
        lv_obj_add_event_cb(slider, fan_vslider_event_cb,
                            LV_EVENT_PRESSED, page);
        lv_obj_add_event_cb(slider, fan_vslider_event_cb,
                            LV_EVENT_PRESSING, page);
        lv_obj_add_event_cb(slider, fan_vslider_event_cb,
                            LV_EVENT_VALUE_CHANGED, page);
        lv_obj_add_event_cb(slider, fan_vslider_event_cb,
                            LV_EVENT_RELEASED, page);
        lv_obj_add_event_cb(slider, fan_vslider_event_cb,
                            LV_EVENT_PRESS_LOST, page);
        if (!page->available) {
            lv_obj_add_state(slider, LV_STATE_DISABLED);
            lv_obj_set_style_opa(track, LV_OPA_40, 0);
        }

        page->speed_controls[0] = slider;
        page->speed_control_count = 1;
        fan_update_slider_visual(page);
        return;
    }

    lv_obj_t *track = fan_create_container(parent,
                                           FAN_VSLIDER_X,
                                           FAN_VSLIDER_Y,
                                           FAN_VSLIDER_WIDTH,
                                           FAN_VSLIDER_HEIGHT);
        lv_obj_set_style_bg_color(track, lv_color_hex(
        page->on ? FAN_VSLIDER_COLOR : 0x999999), 0);
    lv_obj_set_style_bg_opa(track, FAN_VSLIDER_BG_OPA, 0);
    lv_obj_set_style_radius(track, FAN_VSLIDER_RADIUS, 0);
    lv_obj_set_style_clip_corner(track, true, 0);

    page->slider_fill = lv_obj_create(track);
    lv_obj_remove_style_all(page->slider_fill);
    lv_obj_remove_flag(page->slider_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(page->slider_fill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(page->slider_fill,
                              lv_color_hex(FAN_VSLIDER_COLOR), 0);
    lv_obj_set_style_bg_opa(page->slider_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(page->slider_fill, FAN_VSLIDER_RADIUS, 0);

    page->slider_handle = lv_obj_create(track);
    lv_obj_remove_style_all(page->slider_handle);
    lv_obj_remove_flag(page->slider_handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(page->slider_handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(page->slider_handle,
                    FAN_VSLIDER_INDICATOR_W,
                    FAN_VSLIDER_INDICATOR_H);
    lv_obj_set_style_bg_color(page->slider_handle,
                              lv_color_hex(FAN_VSLIDER_INDICATOR_CLR), 0);
    lv_obj_set_style_bg_opa(page->slider_handle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(page->slider_handle,
                            FAN_VSLIDER_INDICATOR_R, 0);

    page->slider_tip = lv_obj_create(parent);
    lv_obj_remove_style_all(page->slider_tip);
    lv_obj_remove_flag(page->slider_tip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(page->slider_tip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(page->slider_tip, FAN_VSLIDER_TIP_W, FAN_VSLIDER_TIP_H);
    lv_obj_set_style_radius(page->slider_tip, 7, 0);
    lv_obj_set_style_bg_color(page->slider_tip, lv_color_hex(0x242424), 0);
    lv_obj_set_style_bg_opa(page->slider_tip, LV_OPA_COVER, 0);
    page->slider_tip_label = lv_label_create(page->slider_tip);
    lv_obj_set_style_text_font(page->slider_tip_label,
                               &ha_font_misan_regular_14, 0);
    lv_obj_set_style_text_color(page->slider_tip_label,
                                lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(page->slider_tip_label);
    lv_obj_add_flag(page->slider_tip, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *slider = lv_slider_create(track);
    lv_obj_set_size(slider, FAN_VSLIDER_WIDTH, FAN_VSLIDER_HEIGHT);
    lv_obj_set_pos(slider, 0, 0);
    lv_slider_set_orientation(slider, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, page->percentage, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, fan_vslider_event_cb, LV_EVENT_PRESSED, page);
    lv_obj_add_event_cb(slider, fan_vslider_event_cb, LV_EVENT_PRESSING, page);
    lv_obj_add_event_cb(slider, fan_vslider_event_cb, LV_EVENT_VALUE_CHANGED, page);
    lv_obj_add_event_cb(slider, fan_vslider_event_cb, LV_EVENT_RELEASED, page);
    lv_obj_add_event_cb(slider, fan_vslider_event_cb, LV_EVENT_PRESS_LOST, page);
    if (!page->available) {
        lv_obj_add_state(slider, LV_STATE_DISABLED);
        lv_obj_set_style_opa(track, LV_OPA_40, 0);
    }

    page->speed_controls[0] = slider;
    page->speed_control_count = 1;
    fan_update_slider_visual(page);
}

static void fan_pwr_btn_event_cb(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    if (!page || !page->available) {
        return;
    }

    fan_call_toggle(page->entity_id);
    page->on = !page->on;

    if (!page->on) {
        fan_animate_slider(page, 0);
    }
    else {
        int pct = page->percentage;
                if (pct <= 0 && page->session &&
            page->session->last_percentage_initialized) {
            pct = page->session->last_percentage;
        }
        if (pct <= 0) {
            pct = 50;
        }
        fan_animate_slider(page, pct);
    }
}

static void fan_build_power_button(lv_obj_t *parent, fan_more_info_page_t *page)
{
    const bool landscape = bsp_lcd_is_landscape();
    const int btn_x = landscape ? FAN_PWR_BTN_X_LANDSCAPE : FAN_PWR_BTN_X;
    const int btn_y = landscape ? FAN_PWR_BTN_Y_LANDSCAPE : FAN_PWR_BTN_Y;
    const int btn_s = landscape ? FAN_PWR_BTN_SIZE_LANDSCAPE
                                : FAN_PWR_BTN_SIZE;
    const int btn_r = landscape ? FAN_PWR_BTN_RADIUS_LANDSCAPE
                                : FAN_PWR_BTN_RADIUS;

    lv_obj_t *btn = fan_create_container(parent, btn_x, btn_y, btn_s, btn_s);
    lv_obj_set_style_radius(btn, btn_r, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(FAN_PWR_BTN_BG), 0);
    lv_obj_set_style_bg_opa(btn, FAN_PWR_BTN_BG_OPA, 0);
    lv_obj_set_style_pad_all(btn, 4, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, MDI_SYMBOL_POWER);
    lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(FAN_PWR_BTN_ICON), 0);
    lv_obj_center(icon);

    lv_obj_add_event_cb(btn, fan_pwr_btn_event_cb, LV_EVENT_CLICKED, page);
    page->pwr_btn = btn;
    if (!page->available) {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_opa(btn, LV_OPA_40, 0);
    }
}

static void fan_bottom_choice_event_cb_v2(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    lv_obj_t *row = lv_event_get_current_target_obj(event);
    if (!page || !row || !page->available) {
        return;
    }

    const uintptr_t encoded = (uintptr_t)lv_obj_get_user_data(row);
    const int idx = (int)((encoded >> 8) & 0xff);
    const int option = (int)(encoded & 0xff);
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        fan_close_bottom_popup_v2(page);
        return;
    }

    if (idx == FAN_BOTTOM_IDX_PRESET && option < page->preset_count) {
        page->active_preset = option;
        cJSON_AddStringToObject(data, "preset_mode", page->presets[option]);
        fan_call_service_data(page->entity_id, "set_preset_mode", data);
        if (page->bottom_value_labels[idx]) {
            fan_label_set_display_text(page->bottom_value_labels[idx], page->presets[option]);
        }
    }
    else if (idx == FAN_BOTTOM_IDX_DIR && option <= 1) {
        page->direction_forward = option == 0;
        cJSON_AddStringToObject(data,
                                "direction",
                                page->direction_forward ? "forward" : "reverse");
        fan_call_service_data(page->entity_id, "set_direction", data);
        if (page->bottom_value_labels[idx]) {
            lv_label_set_text(page->bottom_value_labels[idx],
                              page->direction_forward
                                  ? i18n_get(I18N_FAN_FORWARD)
                                  : i18n_get(I18N_FAN_REVERSE));
        }
        if (page->bottom_icon_labels[idx]) {
            lv_label_set_text(page->bottom_icon_labels[idx],
                              page->direction_forward
                                  ? MDI_SYMBOL_ROTATE_RIGHT
                                  : MDI_SYMBOL_ROTATE_LEFT);
        }
    }
    else if (idx == FAN_BOTTOM_IDX_OSC && option <= 1) {
        page->oscillating = option == 0;
        if (page->session) {
            page->session->last_oscillating = page->oscillating;
            page->session->last_oscillating_initialized = true;
        }
        cJSON_AddBoolToObject(data, "oscillating", page->oscillating);
        fan_call_service_data(page->entity_id, "oscillate", data);
        if (page->bottom_value_labels[idx]) {
            lv_label_set_text(page->bottom_value_labels[idx],
                              page->oscillating
                                  ? i18n_get(I18N_STATE_ON)
                                  : i18n_get(I18N_STATE_OFF));
        }
        if (page->bottom_icon_labels[idx]) {
            lv_label_set_text(page->bottom_icon_labels[idx],
                              page->oscillating
                                  ? MDI_SYMBOL_ARROW_OSCILLATING
                                  : MDI_SYMBOL_ARROW_OSCILLATING_OFF);
        }
    }

    cJSON_Delete(data);
    fan_close_bottom_popup_v2(page);
}

static const char *fan_bottom_option_text_v2(const fan_more_info_page_t *page,
                                             int idx,
                                             int option)
{
    if (idx == FAN_BOTTOM_IDX_PRESET) {
        return page->presets[option];
    }
    if (idx == FAN_BOTTOM_IDX_DIR) {
        return option == 0 ? i18n_get(I18N_FAN_FORWARD)
                           : i18n_get(I18N_FAN_REVERSE);
    }
    return option == 0 ? i18n_get(I18N_OPTION_FAN_YES)
                        : i18n_get(I18N_OPTION_FAN_NO);
}

static const char *fan_bottom_option_icon_v2(int idx, int option)
{
    if (idx == FAN_BOTTOM_IDX_DIR) {
        return option == 0 ? MDI_SYMBOL_ROTATE_RIGHT
                           : MDI_SYMBOL_ROTATE_LEFT;
    }
    if (idx == FAN_BOTTOM_IDX_OSC) {
        return option == 0 ? MDI_SYMBOL_ARROW_OSCILLATING
                           : MDI_SYMBOL_ARROW_OSCILLATING_OFF;
    }
    return NULL;
}

static bool fan_bottom_option_selected_v2(const fan_more_info_page_t *page,
                                          int idx,
                                          int option)
{
    if (idx == FAN_BOTTOM_IDX_PRESET) {
        return option == page->active_preset;
    }
    if (idx == FAN_BOTTOM_IDX_DIR) {
        return (option == 0) == page->direction_forward;
    }
    return (option == 0) == page->oscillating;
}

static int fan_get_active_features_count(fan_more_info_page_t *page,
                                          int *indices)
{
    int count = 0;
    if (page->has_presets && page->preset_count > 0) {
        if (indices) indices[count] = FAN_BOTTOM_IDX_PRESET;
        count++;
    }
    if (page->has_direction) {
        if (indices) indices[count] = FAN_BOTTOM_IDX_DIR;
        count++;
    }
    if (page->has_oscillation) {
        if (indices) indices[count] = FAN_BOTTOM_IDX_OSC;
        count++;
    }
    return count;
}

static void fan_backdrop_click_cb(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    fan_close_bottom_popup_v2(page);
}

static void fan_open_bottom_popup_v2(fan_more_info_page_t *page, int idx)
{
    if (!page || !page->root) {
        return;
    }
    if (page->bottom_popup) {
        const bool same_item = page->open_bottom_idx == idx;
        fan_close_bottom_popup_v2(page);
        if (same_item) {
            return;
        }
    }

    const bool landscape = bsp_lcd_is_landscape();

    int option_count = 2;
    if (idx == FAN_BOTTOM_IDX_PRESET) {
        option_count = page->preset_count;
    }
    if (option_count <= 0) {
        return;
    }

    lv_obj_t *btn = page->bottom_btns[idx];
    int btn_width = lv_obj_get_width(btn);

    lv_obj_set_style_bg_color(btn, lv_color_hex(FAN_BOTTOM_BG_ACTIVE), 0);

    int32_t max_text_w = 0;
    lv_text_attributes_t text_attr;
    lv_text_attributes_init(&text_attr);
    for (int option = 0; option < option_count; option++) {
        const char *text = fan_bottom_option_text_v2(page, idx, option);
        const char *icon = fan_bottom_option_icon_v2(idx, option);
        int32_t tw = lv_text_get_width(text, strlen(text),
                                       FAN_BOTTOM_FONT, &text_attr);
        int32_t row_w = tw + (icon ? 32 : 18) + 8;
        if (row_w > max_text_w) max_text_w = row_w;
    }
    int popup_width = LV_MAX(btn_width, (int)max_text_w);
    popup_width = LV_MIN(popup_width, 320);

    int x;
    int backdrop_w, backdrop_h, backdrop_x, backdrop_y;
    int popup_y;

    if (landscape) {
        x = page->bottom_bar_x;
        backdrop_x = 0;
        backdrop_y = 0;
        backdrop_w = page->bottom_bar_x;
        backdrop_h = 320;
    } else {
        x = page->bottom_bar_x + lv_obj_get_x(btn)
            + (btn_width - popup_width) / 2;
        backdrop_x = 0;
        backdrop_y = 0;
        backdrop_w = 320;
        backdrop_h = FAN_BOTTOM_BAR_Y;
    }

    lv_obj_t *backdrop = lv_obj_create(page->root);
    page->bottom_backdrop = backdrop;
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_pos(backdrop, backdrop_x, backdrop_y);
    lv_obj_set_size(backdrop, backdrop_w, backdrop_h);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(backdrop, fan_backdrop_click_cb, LV_EVENT_CLICKED, page);

    const int natural_h = option_count * FAN_BOTTOM_POPUP_ROW_H;
    const int max_popup_h = landscape ? 320 : FAN_BOTTOM_BAR_Y;
    const int popup_height = LV_MIN(natural_h, max_popup_h);
    if (landscape) {
        lv_obj_t *bar = lv_obj_get_parent(btn);
        popup_y = lv_obj_get_y(bar) + lv_obj_get_y(btn) - popup_height;
        popup_y = LV_CLAMP(0, popup_y, 320 - popup_height);
    } else {
        popup_y = FAN_BOTTOM_BAR_Y - popup_height;
    }
    lv_obj_t *popup = lv_obj_create(page->root);
    page->bottom_popup = popup;
    page->open_bottom_idx = idx;
    lv_obj_add_flag(popup, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(popup,
                   x,
                   popup_y);
    lv_obj_set_size(popup, popup_width, popup_height);
    lv_obj_set_style_radius(popup, FAN_BOTTOM_ITEM_RADIUS, 0);
    lv_obj_set_style_border_width(popup, 1, 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x3A4048), 0);
    lv_obj_set_style_bg_color(popup, lv_color_hex(FAN_BOTTOM_POPUP_BG), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(popup, true, 0);
    lv_obj_set_style_pad_all(popup, 0, 0);
    lv_obj_set_style_pad_gap(popup, 0, 0);
    lv_obj_add_flag(popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_layout(popup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    if (natural_h > (landscape ? 320 : FAN_BOTTOM_BAR_Y)) {
        lv_obj_set_scroll_dir(popup, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(popup, LV_SCROLLBAR_MODE_AUTO);
    }
    else {
        lv_obj_remove_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(popup, LV_SCROLLBAR_MODE_OFF);
    }

    for (int option = 0; option < option_count; option++) {
        lv_obj_t *row = lv_obj_create(popup);
        lv_obj_remove_style_all(row);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, FAN_BOTTOM_POPUP_ROW_H);
        lv_obj_set_style_min_height(row, FAN_BOTTOM_POPUP_ROW_H, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_top(row, 6, 0);
        lv_obj_set_style_pad_bottom(row, 6, 0);
        lv_obj_set_style_pad_left(row, 4, 0);
        lv_obj_set_style_pad_right(row, 4, 0);
        if (fan_bottom_option_selected_v2(page, idx, option)) {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x343941), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        }

        const char *icon_src = fan_bottom_option_icon_v2(idx, option);
        if (icon_src) {
            lv_obj_t *icon = lv_label_create(row);
            lv_label_set_text(icon, icon_src);
            lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
            lv_obj_set_style_text_color(icon,
                                        lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_pos(icon, 4, 10);
            lv_obj_set_size(icon, 20, 20);
            lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
        }
        else {
            lv_obj_t *dot = lv_obj_create(row);
            lv_obj_remove_style_all(dot);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_pos(dot, 4, 17);
            lv_obj_set_size(dot, 6, 6);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(dot,
                                   fan_bottom_option_selected_v2(page, idx, option)
                                       ? LV_OPA_COVER : LV_OPA_40,
                                   0);
        }

        lv_obj_t *label = lv_label_create(row);
        ui_i18n_label_set_text(label, fan_bottom_option_text_v2(page, idx, option));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(label, icon_src ? 28 : 14, 8);
        lv_obj_set_size(label, icon_src ? popup_width - 32 : popup_width - 18, 24);
        lv_obj_set_style_text_font(label, FAN_BOTTOM_FONT, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);

        lv_obj_set_user_data(row,
                             (void *)(uintptr_t)(((uint32_t)idx << 8) |
                                                (uint32_t)option));
        lv_obj_add_event_cb(row,
                            fan_bottom_choice_event_cb_v2,
                            LV_EVENT_CLICKED,
                            page);
    }
    lv_obj_move_foreground(popup);
}

static void fan_bottom_item_event_cb_v2(lv_event_t *event)
{
    fan_more_info_page_t *page = lv_event_get_user_data(event);
    lv_obj_t *btn = lv_event_get_current_target_obj(event);
    if (!page || !btn || !page->available) {
        return;
    }
    fan_open_bottom_popup_v2(page,
                             (int)(uintptr_t)lv_obj_get_user_data(btn));
}

static lv_obj_t *fan_create_bottom_item_v2(lv_obj_t *parent,
                                           int width,
                                           const char *title,
                                           const char *icon_src,
                                           int idx,
                                           bool enabled,
                                           fan_more_info_page_t *page)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, width, FAN_BOTTOM_BAR_HEIGHT);
    lv_obj_set_style_radius(btn, FAN_BOTTOM_ITEM_RADIUS, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(FAN_BOTTOM_BG_DEFAULT), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_ver(btn, FAN_BOTTOM_ITEM_PAD_VER, 0);
    lv_obj_set_style_pad_hor(btn, FAN_BOTTOM_ITEM_PAD_HOR, 0);

    lv_obj_t *top = fan_create_container(btn, 0, 0, width - 20, 20);
    lv_obj_set_layout(top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon = lv_label_create(top);
    lv_label_set_text(icon, icon_src);
    lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(FAN_BOTTOM_TEXT_COLOR), 0);
    lv_obj_set_size(icon, FAN_BOTTOM_ICON_SIZE, FAN_BOTTOM_ICON_SIZE);
    page->bottom_icon_labels[idx] = icon;

    lv_obj_t *title_label = lv_label_create(btn);
    ui_i18n_label_set_text(title_label, title);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(title_label, 0, 20);
    lv_obj_set_size(title_label, width - 20, 22);
    lv_obj_set_style_text_font(title_label, FAN_BOTTOM_FONT, 0);
    lv_obj_set_style_text_color(title_label,
                                lv_color_hex(FAN_BOTTOM_TEXT_COLOR), 0);

    page->bottom_btns[idx] = btn;

    if (enabled && page->available) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(btn, (void *)(uintptr_t)idx);
        lv_obj_add_event_cb(btn,
                            fan_bottom_item_event_cb_v2,
                            LV_EVENT_CLICKED,
                            page);
    }
    else {
        lv_obj_set_style_opa(btn, LV_OPA_40, 0);
    }
    return btn;
}

static void fan_build_bottom_bar_v2(lv_obj_t *parent,
                                    fan_more_info_page_t *page)
{
    int active_indices[FAN_BOTTOM_COUNT];
    int active_count = fan_get_active_features_count(page, active_indices);

    if (active_count == 0) {
        return;
    }

    const bool landscape = bsp_lcd_is_landscape();
    int bar_x, bar_width, bar_y, bar_height;

    if (landscape) {
        bar_x = FAN_BOTTOM_BAR_X_LANDSCAPE;
        bar_width = FAN_BOTTOM_BAR_W_LANDSCAPE;
        if (active_count == 3) {
            bar_y = FAN_BOTTOM_BAR_Y3_LANDSCAPE;
            bar_height = FAN_BOTTOM_BAR_H3_LANDSCAPE;
        } else if (active_count == 2) {
            bar_y = FAN_BOTTOM_BAR_Y2_LANDSCAPE;
            bar_height = FAN_BOTTOM_BAR_H2_LANDSCAPE;
        } else {
            bar_y = FAN_BOTTOM_BAR_Y1_LANDSCAPE;
            bar_height = FAN_BOTTOM_BAR_H1_LANDSCAPE;
        }
        page->bottom_bar_x = bar_x;

        lv_obj_t *bar = fan_create_container(parent,
                                             bar_x, bar_y,
                                             bar_width, bar_height);
        lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(bar,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(bar, FAN_BOTTOM_BAR_GAP_LANDSCAPE, 0);

        for (int i = 0; i < active_count; i++) {
            int idx = active_indices[i];
            const int item_height = (bar_height -
                                     (active_count - 1) *
                                         FAN_BOTTOM_BAR_GAP_LANDSCAPE) /
                                    active_count;

            const char *title;
            const char *value;
            const char *icon;
            bool enabled;

            if (idx == FAN_BOTTOM_IDX_PRESET) {
                title = i18n_get(I18N_FAN_PRESET_MODE);
                icon = MDI_SYMBOL_TUNE_VARIANT;
                enabled = true;
            } else if (idx == FAN_BOTTOM_IDX_DIR) {
                title = i18n_get(I18N_FAN_DIRECTION_LABEL);
                icon = page->direction_forward
                                  ? MDI_SYMBOL_ROTATE_RIGHT
                                  : MDI_SYMBOL_ROTATE_LEFT;
                enabled = page->has_direction;
            } else {
                title = i18n_get(I18N_FAN_OSCILLATING_LABEL);
                icon = page->oscillating
                                  ? MDI_SYMBOL_ARROW_OSCILLATING
                                  : MDI_SYMBOL_ARROW_OSCILLATING_OFF;
                enabled = page->has_oscillation;
            }

            fan_create_bottom_item_v2(bar, bar_width,
                                      title, icon,
                                      idx, enabled, page);
        }
        return;
    }

    if (active_count == 1) {
        bar_width = FAN_BOTTOM_ITEM_SINGLE_W;
        bar_x = FAN_BOTTOM_ITEM_SINGLE_X;
    } else if (active_count == 2) {
        bar_width = FAN_BOTTOM_ITEM_SINGLE_W * 2 + FAN_BOTTOM_BAR_GAP;
        bar_x = FAN_BOTTOM_ITEM_DUAL_X;
    } else {
        bar_width = FAN_BOTTOM_BAR_WIDTH;
        bar_x = FAN_BOTTOM_BAR_X;
    }
    page->bottom_bar_x = bar_x;

    lv_obj_t *bar = fan_create_container(parent,
                                         bar_x,
                                         FAN_BOTTOM_BAR_Y,
                                         bar_width,
                                         FAN_BOTTOM_BAR_HEIGHT);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(bar, FAN_BOTTOM_BAR_GAP, 0);

    for (int i = 0; i < active_count; i++) {
        int idx = active_indices[i];
        int item_width;

        if (active_count == 3) {
            if (idx == FAN_BOTTOM_IDX_PRESET) item_width = 103;
            else if (idx == FAN_BOTTOM_IDX_DIR) item_width = 79;
            else item_width = 88;
        } else {
            item_width = FAN_BOTTOM_ITEM_SINGLE_W;
        }

        const char *title;
        const char *value;
        const char *icon;
        bool enabled;

        if (idx == FAN_BOTTOM_IDX_PRESET) {
            title = i18n_get(I18N_FAN_PRESET_MODE);
            icon = MDI_SYMBOL_TUNE_VARIANT;
            enabled = true;
        } else if (idx == FAN_BOTTOM_IDX_DIR) {
            title = i18n_get(I18N_FAN_DIRECTION_LABEL);
            icon = page->direction_forward
                              ? MDI_SYMBOL_ROTATE_RIGHT
                              : MDI_SYMBOL_ROTATE_LEFT;
            enabled = page->has_direction;
        } else {
            title = i18n_get(I18N_FAN_OSCILLATING_LABEL);
            icon = page->oscillating
                              ? MDI_SYMBOL_ARROW_OSCILLATING
                              : MDI_SYMBOL_ARROW_OSCILLATING_OFF;
            enabled = page->has_oscillation;
        }

        fan_create_bottom_item_v2(bar, item_width, title, icon,
                                  idx, enabled, page);
    }
}

/* ---------------------------------------------------------------------------
 * More Info page: main render entry
 * ------------------------------------------------------------------------- */

static esp_err_t fan_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->store || !context->close) {
        return ESP_ERR_INVALID_ARG;
    }

    fan_more_info_page_t *page = lv_calloc(1, sizeof(*page));
    if (!page) {
        return ESP_ERR_NO_MEM;
    }
    page->session = fan_get_session(context->screen);
    if (!page->session) {
        lv_free(page);
        return ESP_ERR_NO_MEM;
    }
    fan_read_page_state(context, page);
    if (!page->on) {
        page->percentage = 0;
    }
    s_active_fan_page = page;

    // Screen setup
    lv_obj_t *screen = context->screen;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x010203), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *root = fan_create_container(screen, 0, 0,
                                          bsp_lcd_is_landscape() ? 480 : 320,
                                          bsp_lcd_is_landscape() ? 320 : 480);
    page->root = root;
    page->open_bottom_idx = -1;
    lv_obj_add_event_cb(root, fan_page_delete_cb, LV_EVENT_DELETE, page);

    // 1. Header (Top)
    fan_build_header(root, context->overlay, context);

    // 2. State Label (Below header)
    fan_build_state_block(root, context, page);

    // 3. Main control selected from the fan's HA capabilities. Keep the
    // original continuous percentage slider as its own control type.
    const bool supports_set_speed =
        (page->supported_features & FAN_FEATURE_SET_SPEED) != 0;
    const bool has_bottom_features =
        fan_get_active_features_count(page, NULL) > 0;

    /* Use HA's computeFanSpeedCount: round(100 / percentage_step) + 1.
       speedCount > FAN_HA_SPEED_COUNT_MAX (6) → slider; else → buttons. */
    int ha_speed_count = 0;
    if (page->percentage_step > 0.0f) {
        ha_speed_count = (int)(100.0f / page->percentage_step + 0.5f) + 1;
    }

    if (supports_set_speed && ha_speed_count > 2 &&
        ha_speed_count < FAN_HA_SPEED_COUNT_MAX) {
        page->speed_count = ha_speed_count - 1;
        fan_build_speed_buttons(root, page);
    }
    else if (supports_set_speed && ha_speed_count == 2) {
        page->speed_count = ha_speed_count - 1;
        fan_build_labeled_switch(root, page);
    }
    else if (supports_set_speed) {
        fan_build_vertical_slider(root, page);
        fan_build_power_button(root, page);
    }
    else if (page->has_percentage || has_bottom_features) {
        fan_build_icon_switch(root, page);
    }
    else {
        fan_build_labeled_switch(root, page);
    }

    // 4. Bottom pull-up selectors (zero to three depending on HA features).
    fan_build_bottom_bar_v2(root, page);

    return ESP_OK;
}

const entity_handler_t fan_entity_handler = {
    .domain = "fan",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_FAN,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_TOGGLE },
    },
    .handle_action = fan_handle_action,
    .render_tile = fan_render_tile,
    .render_more_info = fan_render_more_info,
    .update_more_info = fan_more_info_update,
};
