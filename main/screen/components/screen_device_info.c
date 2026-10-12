#include "screen_device_info.h"

#include <string.h>

#include "../../assets/ha_ui_assets.h"
#include "system_status_bar.h"

#define SCREEN_DEVICE_INFO_CARD_WIDTH    296
#define SCREEN_DEVICE_INFO_INFO_HEIGHT   92
#define SCREEN_DEVICE_INFO_OFFLINE_HEIGHT 60
#define SCREEN_DEVICE_INFO_VERSION_COMPACT_HEIGHT 109
#define SCREEN_DEVICE_INFO_VERSION_RESULT_HEIGHT 151
#define SCREEN_DEVICE_INFO_ANIMATION_MS  250

#define SCREEN_DEVICE_INFO_BG_COLOR      0x010203
#define SCREEN_DEVICE_INFO_CARD_COLOR    0x262626
#define SCREEN_DEVICE_INFO_PRIMARY_COLOR 0xFFFFFF
#define SCREEN_DEVICE_INFO_SECONDARY_COLOR 0xA8A8A8
#define SCREEN_DEVICE_INFO_BUTTON_COLOR  0x0D84F7

struct screen_device_info {
    lv_obj_t *root;
    system_status_bar_t *status_bar;
    lv_obj_t *wifi_card;
    lv_obj_t *wifi_value_label;
    lv_obj_t *wifi_detail_label;
    lv_obj_t *ap_card;
    lv_obj_t *ap_value_label;
    lv_obj_t *ap_detail_label;
    lv_obj_t *version_card;
    lv_obj_t *version_value_label;
    lv_obj_t *check_update_button;
    lv_obj_t *check_update_label;
    lv_obj_t *loading_icon;
    lv_obj_t *update_result_row;
    lv_obj_t *up_to_date_icon;
    lv_obj_t *up_to_date_label;
    lv_obj_t *available_dot;
    lv_obj_t *available_version_label;
    lv_obj_t *upgrade_label;
    lv_obj_t *check_failed_icon;
    lv_obj_t *check_failed_label;
    screen_device_info_check_update_cb_t check_update_cb;
    void *check_update_user_data;
    screen_device_info_upgrade_cb_t upgrade_cb;
    void *upgrade_user_data;
    screen_device_info_hidden_cb_t hidden_cb;
    void *hidden_user_data;
    bool checking;
    screen_device_info_update_state_t update_state;
};

static void screen_device_info_apply_clear_style(lv_obj_t *obj);
static void screen_device_info_apply_bold_text(lv_obj_t *label, lv_color_t color);
static lv_obj_t *screen_device_info_create_info_card(lv_obj_t *parent,
                                                      const char *label,
                                                      const char *value,
                                                      const char *detail,
                                                      lv_obj_t **value_label,
                                                      lv_obj_t **detail_label);
static void screen_device_info_create_version_card(screen_device_info_t *view,
                                                    lv_obj_t *parent);
static void screen_device_info_set_connection_card(lv_obj_t *card,
                                                    lv_obj_t *value_label,
                                                    lv_obj_t *detail_label,
                                                    bool active,
                                                    const char *active_value,
                                                    const char *active_detail,
                                                    const char *inactive_value);
static void screen_device_info_set_label_text(lv_obj_t *label, const char *text);
static void screen_device_info_hide_update_results(screen_device_info_t *view);
static void screen_device_info_check_update_event_cb(lv_event_t *event);
static void screen_device_info_upgrade_event_cb(lv_event_t *event);
static void screen_device_info_set_loading_rotation(void *obj, int32_t angle);
static void screen_device_info_set_translate_y(void *obj, int32_t y);
static void screen_device_info_hide_animation_completed(lv_anim_t *animation);

screen_device_info_t *screen_device_info_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    screen_device_info_t *view = lv_malloc(sizeof(screen_device_info_t));
    if (!view) {
        return NULL;
    }
    memset(view, 0, sizeof(*view));

    view->root = lv_obj_create(parent);
    if (!view->root) {
        lv_free(view);
        return NULL;
    }

    lv_obj_remove_flag(view->root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_size(view->root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(view->root, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(view->root, 0, 0);
    lv_obj_set_style_border_width(view->root, 0, 0);
    lv_obj_set_style_bg_color(view->root, lv_color_hex(SCREEN_DEVICE_INFO_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(view->root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(view->root, 0, 0);

    view->status_bar = system_status_bar_create(view->root);

    lv_obj_t *content = lv_obj_create(view->root);
    screen_device_info_apply_clear_style(content);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(content, LV_PCT(100), LV_VER_RES - SYSTEM_STATUS_BAR_HEIGHT);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, SYSTEM_STATUS_BAR_HEIGHT);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(content, 10, 0);
    lv_obj_set_style_pad_bottom(content, 10, 0);
    lv_obj_set_style_pad_row(content, 8, 0);

    view->wifi_card = screen_device_info_create_info_card(content,
                                                           "Wi-Fi",
                                                           "Not Connected",
                                                           "",
                                                           &view->wifi_value_label,
                                                           &view->wifi_detail_label);
    view->ap_card = screen_device_info_create_info_card(content,
                                                         "AP",
                                                         "Disabled",
                                                         "",
                                                         &view->ap_value_label,
                                                         &view->ap_detail_label);
    screen_device_info_create_version_card(view, content);

    /* Preserve the layout position and move the overlay independently of the
     * parent layout. Otherwise a layout refresh can reset an animated y. */
    lv_obj_set_style_translate_y(view->root, -LV_VER_RES, 0);
    lv_obj_add_flag(view->root, LV_OBJ_FLAG_HIDDEN);
    return view;
}

void screen_device_info_destroy(screen_device_info_t *view)
{
    if (!view) {
        return;
    }

    view->root = NULL;
    lv_free(view);
}

lv_obj_t *screen_device_info_get_obj(screen_device_info_t *view)
{
    return view ? view->root : NULL;
}

void screen_device_info_set_data(screen_device_info_t *view,
                                 const screen_device_info_data_t *data)
{
    if (!view || !data) {
        return;
    }

    screen_device_info_set_connection_card(view->wifi_card,
                                            view->wifi_value_label,
                                            view->wifi_detail_label,
                                            data->wifi_connected,
                                            data->wifi_ssid,
                                            data->wifi_ip,
                                            "Not Connected");
    screen_device_info_set_connection_card(view->ap_card,
                                            view->ap_value_label,
                                            view->ap_detail_label,
                                            data->ap_enabled,
                                            data->ap_ssid,
                                            data->ap_ip,
                                            "Disabled");

    screen_device_info_set_label_text(view->version_value_label,
                                      data->firmware_version[0] ? data->firmware_version : "--");
    system_status_bar_set_time(view->status_bar,
                               data->time_text[0] ? data->time_text : "--:--");
}

void screen_device_info_set_check_update_cb(
    screen_device_info_t *view,
    screen_device_info_check_update_cb_t callback,
    void *user_data)
{
    if (!view) {
        return;
    }
    view->check_update_cb = callback;
    view->check_update_user_data = user_data;
}

void screen_device_info_set_upgrade_cb(screen_device_info_t *view,
                                       screen_device_info_upgrade_cb_t callback,
                                       void *user_data)
{
    if (!view) {
        return;
    }
    view->upgrade_cb = callback;
    view->upgrade_user_data = user_data;
}

void screen_device_info_set_hidden_cb(screen_device_info_t *view,
                                      screen_device_info_hidden_cb_t callback,
                                      void *user_data)
{
    if (!view) {
        return;
    }
    view->hidden_cb = callback;
    view->hidden_user_data = user_data;
}

void screen_device_info_set_checking(screen_device_info_t *view, bool checking)
{
    screen_device_info_set_update_state(
        view,
        checking ? SCREEN_DEVICE_INFO_UPDATE_CHECKING
                 : SCREEN_DEVICE_INFO_UPDATE_IDLE,
        NULL);
}

void screen_device_info_set_update_state(
    screen_device_info_t *view,
    screen_device_info_update_state_t state,
    const char *available_version)
{
    if (!view || !view->check_update_button || !view->check_update_label ||
        !view->loading_icon || !view->update_result_row) {
        return;
    }

    bool checking = state == SCREEN_DEVICE_INFO_UPDATE_CHECKING;
    bool has_result = state == SCREEN_DEVICE_INFO_UPDATE_UP_TO_DATE ||
                      state == SCREEN_DEVICE_INFO_UPDATE_AVAILABLE ||
                      state == SCREEN_DEVICE_INFO_UPDATE_STAGED ||
                      state == SCREEN_DEVICE_INFO_UPDATE_CHECK_FAILED;
    view->update_state = state;
    screen_device_info_hide_update_results(view);
    lv_obj_set_height(view->version_card,
                      has_result ? SCREEN_DEVICE_INFO_VERSION_RESULT_HEIGHT
                                 : SCREEN_DEVICE_INFO_VERSION_COMPACT_HEIGHT);

    if (checking != view->checking) {
        view->checking = checking;
        lv_anim_delete(view->loading_icon, screen_device_info_set_loading_rotation);
        if (checking) {
            lv_image_set_rotation(view->loading_icon, 0);
            lv_obj_remove_flag(view->loading_icon, LV_OBJ_FLAG_HIDDEN);

            lv_anim_t animation;
            lv_anim_init(&animation);
            lv_anim_set_var(&animation, view->loading_icon);
            lv_anim_set_exec_cb(&animation, screen_device_info_set_loading_rotation);
            lv_anim_set_values(&animation, 0, 3600);
            lv_anim_set_duration(&animation, 1000);
            lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&animation);
        }
        else {
            lv_image_set_rotation(view->loading_icon, 0);
            lv_obj_add_flag(view->loading_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (checking) {
        lv_label_set_text(view->check_update_label, "Checking");
        lv_obj_remove_flag(view->check_update_button, LV_OBJ_FLAG_CLICKABLE);
        return;
    }

    lv_label_set_text(view->check_update_label, "Check Update");
    lv_obj_add_flag(view->check_update_button, LV_OBJ_FLAG_CLICKABLE);

    switch (state) {
    case SCREEN_DEVICE_INFO_UPDATE_UP_TO_DATE:
        lv_obj_remove_flag(view->update_result_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->up_to_date_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->up_to_date_label, LV_OBJ_FLAG_HIDDEN);
        break;
    case SCREEN_DEVICE_INFO_UPDATE_AVAILABLE:
    case SCREEN_DEVICE_INFO_UPDATE_STAGED:
        screen_device_info_set_label_text(
            view->available_version_label,
            available_version && available_version[0] ? available_version : "New version");
        lv_obj_remove_flag(view->update_result_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->available_dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->available_version_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->upgrade_label, LV_OBJ_FLAG_HIDDEN);
        break;
    case SCREEN_DEVICE_INFO_UPDATE_CHECK_FAILED:
        lv_obj_remove_flag(view->update_result_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->check_failed_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->check_failed_label, LV_OBJ_FLAG_HIDDEN);
        break;
    case SCREEN_DEVICE_INFO_UPDATE_IDLE:
    case SCREEN_DEVICE_INFO_UPDATE_CHECKING:
    default:
        break;
    }
}

void screen_device_info_show(screen_device_info_t *view)
{
    if (!view || !view->root) {
        return;
    }

    lv_anim_delete(view->root, screen_device_info_set_translate_y);
    lv_obj_move_to_index(view->root, -1);
    lv_obj_set_style_translate_y(view->root, -LV_VER_RES, 0);
    lv_obj_remove_flag(view->root, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view->root);
    lv_anim_set_exec_cb(&animation, screen_device_info_set_translate_y);
    lv_anim_set_values(&animation, -LV_VER_RES, 0);
    lv_anim_set_duration(&animation, SCREEN_DEVICE_INFO_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

void screen_device_info_hide(screen_device_info_t *view)
{
    if (!view || !view->root) {
        return;
    }

    if (lv_obj_has_flag(view->root, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    lv_anim_delete(view->root, screen_device_info_set_translate_y);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view->root);
    lv_anim_set_exec_cb(&animation, screen_device_info_set_translate_y);
    lv_anim_set_values(&animation,
                       lv_obj_get_style_translate_y(view->root, LV_PART_MAIN),
                       -LV_VER_RES);
    lv_anim_set_duration(&animation, SCREEN_DEVICE_INFO_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
    lv_anim_set_user_data(&animation, view);
    lv_anim_set_completed_cb(&animation, screen_device_info_hide_animation_completed);
    lv_anim_start(&animation);
}

bool screen_device_info_is_visible(const screen_device_info_t *view)
{
    return view && view->root && !lv_obj_has_flag(view->root, LV_OBJ_FLAG_HIDDEN);
}

static void screen_device_info_apply_clear_style(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void screen_device_info_apply_bold_text(lv_obj_t *label, lv_color_t color)
{
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, &ha_font_misan_medium_20, 0);
}

static lv_obj_t *screen_device_info_create_info_card(lv_obj_t *parent,
                                                      const char *label,
                                                      const char *value,
                                                      const char *detail,
                                                      lv_obj_t **value_label_out,
                                                      lv_obj_t **detail_label_out)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(card, SCREEN_DEVICE_INFO_CARD_WIDTH, SCREEN_DEVICE_INFO_INFO_HEIGHT);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SCREEN_DEVICE_INFO_CARD_COLOR), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_column(card, 8, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label_obj = lv_label_create(card);
    lv_obj_set_width(label_obj, 88);
    lv_label_set_text(label_obj, label ? label : "");
    screen_device_info_apply_bold_text(label_obj,
                                       lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR));

    lv_obj_t *values = lv_obj_create(card);
    screen_device_info_apply_clear_style(values);
    lv_obj_remove_flag(values, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(values, LV_PCT(100));
    lv_obj_set_flex_grow(values, 1);
    lv_obj_set_layout(values, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(values, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(values,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(values, 2, 0);

    lv_obj_t *value_label = lv_label_create(values);
    lv_obj_set_size(value_label,
                    LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_regular_16));
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(value_label, value ? value : "");
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(value_label, lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR), 0);
    lv_obj_set_style_text_font(value_label, &ha_font_misan_regular_16, 0);

    lv_obj_t *detail_label = lv_label_create(values);
    lv_obj_set_size(detail_label,
                    LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_regular_16));
    lv_label_set_long_mode(detail_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(detail_label, detail ? detail : "");
    lv_obj_set_style_text_align(detail_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(detail_label, lv_color_hex(SCREEN_DEVICE_INFO_SECONDARY_COLOR), 0);
    lv_obj_set_style_text_font(detail_label, &ha_font_misan_regular_16, 0);

    if (value_label_out) {
        *value_label_out = value_label;
    }
    if (detail_label_out) {
        *detail_label_out = detail_label;
    }

    return card;
}

static void screen_device_info_create_version_card(screen_device_info_t *view,
                                                    lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    view->version_card = card;
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(card,
                    SCREEN_DEVICE_INFO_CARD_WIDTH,
                    SCREEN_DEVICE_INFO_VERSION_COMPACT_HEIGHT);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SCREEN_DEVICE_INFO_CARD_COLOR), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 7, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *version_row = lv_obj_create(card);
    screen_device_info_apply_clear_style(version_row);
    lv_obj_remove_flag(version_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(version_row, LV_PCT(100), 30);
    lv_obj_set_layout(version_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(version_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(version_row,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(version_row, 10, 0);

    lv_obj_t *version_title = lv_label_create(version_row);
    lv_label_set_text(version_title, "Version");
    screen_device_info_apply_bold_text(version_title,
                                       lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR));

    view->version_value_label = lv_label_create(version_row);
    lv_obj_set_flex_grow(view->version_value_label, 1);
    lv_label_set_long_mode(view->version_value_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(view->version_value_label, "--");
    lv_obj_set_style_text_align(view->version_value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(view->version_value_label, lv_color_hex(SCREEN_DEVICE_INFO_SECONDARY_COLOR), 0);
    lv_obj_set_style_text_font(view->version_value_label, &ha_font_misan_regular_16, 0);

    view->update_result_row = lv_obj_create(card);
    screen_device_info_apply_clear_style(view->update_result_row);
    lv_obj_remove_flag(view->update_result_row,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(view->update_result_row, LV_PCT(100), 32);

    view->up_to_date_icon = lv_image_create(view->update_result_row);
    lv_image_set_src(view->up_to_date_icon, &ok_icon_green);
    lv_obj_align(view->up_to_date_icon, LV_ALIGN_LEFT_MID, 0, 0);

    view->up_to_date_label = lv_label_create(view->update_result_row);
    lv_label_set_text(view->up_to_date_label, "Up to Date");
    lv_obj_set_style_text_color(view->up_to_date_label,
                                lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR),
                                0);
    lv_obj_set_style_text_font(view->up_to_date_label,
                               &ha_font_misan_regular_16,
                               0);
    lv_obj_align(view->up_to_date_label, LV_ALIGN_LEFT_MID, 40, 0);

    view->available_dot = lv_obj_create(view->update_result_row);
    lv_obj_remove_style_all(view->available_dot);
    lv_obj_set_size(view->available_dot, 6, 6);
    lv_obj_set_style_radius(view->available_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(view->available_dot, lv_color_hex(0xFF5A5F), 0);
    lv_obj_set_style_bg_opa(view->available_dot, LV_OPA_COVER, 0);
    lv_obj_align(view->available_dot, LV_ALIGN_LEFT_MID, 0, 0);

    view->available_version_label = lv_label_create(view->update_result_row);
    lv_obj_set_width(view->available_version_label, 168);
    lv_label_set_long_mode(view->available_version_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(view->available_version_label, "New version");
    lv_obj_set_style_text_color(view->available_version_label,
                                lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR),
                                0);
    lv_obj_set_style_text_font(view->available_version_label,
                               &ha_font_misan_regular_16,
                               0);
    lv_obj_align(view->available_version_label, LV_ALIGN_LEFT_MID, 14, 0);

    view->upgrade_label = lv_label_create(view->update_result_row);
    lv_label_set_text(view->upgrade_label, "Upgrade");
    lv_obj_set_style_text_color(view->upgrade_label,
                                lv_color_hex(SCREEN_DEVICE_INFO_BUTTON_COLOR),
                                0);
    lv_obj_set_style_text_font(view->upgrade_label,
                               &ha_font_misan_regular_16,
                               0);
    lv_obj_align(view->upgrade_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(view->upgrade_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(view->upgrade_label, 12);
    lv_obj_add_event_cb(view->upgrade_label,
                        screen_device_info_upgrade_event_cb,
                        LV_EVENT_CLICKED,
                        view);

    view->check_failed_icon = lv_obj_create(view->update_result_row);
    lv_obj_remove_flag(view->check_failed_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(view->check_failed_icon, 32, 32);
    lv_obj_set_style_radius(view->check_failed_icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(view->check_failed_icon, 0, 0);
    lv_obj_set_style_bg_color(view->check_failed_icon, lv_color_hex(0xFF4D55), 0);
    lv_obj_set_style_bg_opa(view->check_failed_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(view->check_failed_icon, 0, 0);
    lv_obj_align(view->check_failed_icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *check_failed_mark = lv_label_create(view->check_failed_icon);
    lv_label_set_text(check_failed_mark, "!");
    lv_obj_set_style_text_color(check_failed_mark,
                                lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR),
                                0);
    lv_obj_set_style_text_font(check_failed_mark, &ha_font_misan_medium_20, 0);
    lv_obj_center(check_failed_mark);

    view->check_failed_label = lv_label_create(view->update_result_row);
    lv_label_set_text(view->check_failed_label, "Check failed.");
    lv_obj_set_style_text_color(view->check_failed_label,
                                lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR),
                                0);
    lv_obj_set_style_text_font(view->check_failed_label,
                               &ha_font_misan_regular_16,
                               0);
    lv_obj_align(view->check_failed_label, LV_ALIGN_LEFT_MID, 40, 0);

    screen_device_info_hide_update_results(view);

    view->check_update_button = lv_button_create(card);
    lv_obj_set_size(view->check_update_button, LV_PCT(100), 48);
    lv_obj_set_style_radius(view->check_update_button, 12, 0);
    lv_obj_set_style_border_width(view->check_update_button, 0, 0);
    lv_obj_set_style_bg_color(view->check_update_button,
                              lv_color_hex(SCREEN_DEVICE_INFO_BUTTON_COLOR),
                              0);
    lv_obj_set_style_bg_opa(view->check_update_button, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(view->check_update_button, 0, 0);
    lv_obj_set_style_pad_all(view->check_update_button, 0, 0);
    lv_obj_add_event_cb(view->check_update_button,
                        screen_device_info_check_update_event_cb,
                        LV_EVENT_CLICKED,
                        view);

    lv_obj_t *button_content = lv_obj_create(view->check_update_button);
    screen_device_info_apply_clear_style(button_content);
    lv_obj_remove_flag(button_content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(button_content, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(button_content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_content,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(button_content, 8, 0);
    lv_obj_center(button_content);

    view->check_update_label = lv_label_create(button_content);
    lv_label_set_text(view->check_update_label, "Check Update");
    screen_device_info_apply_bold_text(view->check_update_label,
                                       lv_color_hex(SCREEN_DEVICE_INFO_PRIMARY_COLOR));

    view->loading_icon = lv_image_create(button_content);
    lv_image_set_src(view->loading_icon, &loading_icon_32);
    lv_image_set_pivot(view->loading_icon, 16, 16);
    lv_obj_add_flag(view->loading_icon, LV_OBJ_FLAG_HIDDEN);
}

static void screen_device_info_set_connection_card(lv_obj_t *card,
                                                    lv_obj_t *value_label,
                                                    lv_obj_t *detail_label,
                                                    bool active,
                                                    const char *active_value,
                                                    const char *active_detail,
                                                    const char *inactive_value)
{
    if (!card || !value_label || !detail_label) {
        return;
    }

    if (active) {
        lv_obj_set_height(card, SCREEN_DEVICE_INFO_INFO_HEIGHT);
        screen_device_info_set_label_text(value_label,
                                          active_value && active_value[0] ? active_value : "--");
        screen_device_info_set_label_text(detail_label,
                                          active_detail && active_detail[0] ? active_detail : "--");
        lv_obj_remove_flag(detail_label, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_set_height(card, SCREEN_DEVICE_INFO_OFFLINE_HEIGHT);
        screen_device_info_set_label_text(value_label,
                                          inactive_value ? inactive_value : "--");
        screen_device_info_set_label_text(detail_label, "");
        lv_obj_add_flag(detail_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void screen_device_info_set_label_text(lv_obj_t *label, const char *text)
{
    if (!label || !text) {
        return;
    }

    const char *current_text = lv_label_get_text(label);
    if (!current_text || strcmp(current_text, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void screen_device_info_hide_update_results(screen_device_info_t *view)
{
    if (!view || !view->update_result_row) {
        return;
    }

    lv_obj_add_flag(view->update_result_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->up_to_date_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->up_to_date_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->available_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->available_version_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->upgrade_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->check_failed_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->check_failed_label, LV_OBJ_FLAG_HIDDEN);
}

static void screen_device_info_check_update_event_cb(lv_event_t *event)
{
    screen_device_info_t *view = lv_event_get_user_data(event);
    if (!view || view->checking || !view->check_update_cb) {
        return;
    }
    view->check_update_cb(view, view->check_update_user_data);
}

static void screen_device_info_upgrade_event_cb(lv_event_t *event)
{
    screen_device_info_t *view = lv_event_get_user_data(event);
    if (!view ||
        (view->update_state != SCREEN_DEVICE_INFO_UPDATE_AVAILABLE &&
         view->update_state != SCREEN_DEVICE_INFO_UPDATE_STAGED) ||
        !view->upgrade_cb) {
        return;
    }
    view->upgrade_cb(view, view->upgrade_user_data);
}

static void screen_device_info_set_loading_rotation(void *obj, int32_t angle)
{
    lv_image_set_rotation((lv_obj_t *)obj, angle);
}

static void screen_device_info_set_translate_y(void *obj, int32_t y)
{
    lv_obj_set_style_translate_y((lv_obj_t *)obj, y, 0);
}

static void screen_device_info_hide_animation_completed(lv_anim_t *animation)
{
    screen_device_info_t *view = lv_anim_get_user_data(animation);
    if (!view || !view->root) {
        return;
    }

    lv_obj_add_flag(view->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_y(view->root, -LV_VER_RES, 0);
    if (view->hidden_cb) {
        view->hidden_cb(view, view->hidden_user_data);
    }
}
