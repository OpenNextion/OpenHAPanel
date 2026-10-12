#include "screen_ota_update.h"

#include <string.h>

#include "../../assets/ha_ui_assets.h"
#include "system_status_bar.h"

#define OTA_BG_COLOR        0x010203
#define OTA_PRIMARY_COLOR   0xFFFFFF
#define OTA_BLUE_COLOR      0x0D84F7
#define OTA_TRACK_COLOR     0x24272C
#define OTA_WARNING_COLOR   0xFF9800
#define OTA_CANCEL_COLOR    0x303030

typedef enum {
    SCREEN_OTA_MODE_NONE = 0,
    SCREEN_OTA_MODE_UPDATING,
    SCREEN_OTA_MODE_FAILED,
    SCREEN_OTA_MODE_RESTARTING,
} screen_ota_mode_t;

struct screen_ota_update {
    lv_obj_t *root;
    system_status_bar_t *status_bar;
    lv_obj_t *updating_content;
    lv_obj_t *progress_arc;
    lv_obj_t *progress_label;
    lv_obj_t *failed_content;
    lv_obj_t *restarting_content;
    screen_ota_update_action_cb_t retry_cb;
    screen_ota_update_action_cb_t cancel_cb;
    void *callback_user_data;
    screen_ota_mode_t mode;
};

static void screen_ota_clear_style(lv_obj_t *obj);
static lv_obj_t *screen_ota_create_content(lv_obj_t *parent);
static lv_obj_t *screen_ota_create_label(lv_obj_t *parent,
                                         const char *text,
                                         const lv_font_t *font,
                                         uint32_t color);
static lv_obj_t *screen_ota_create_warning(lv_obj_t *parent, const char *text);
static lv_obj_t *screen_ota_create_button(lv_obj_t *parent,
                                          const char *text,
                                          uint32_t color);
static void screen_ota_retry_event_cb(lv_event_t *event);
static void screen_ota_cancel_event_cb(lv_event_t *event);
static void screen_ota_show_mode(screen_ota_update_t *view, screen_ota_mode_t mode);

screen_ota_update_t *screen_ota_update_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    screen_ota_update_t *view = lv_malloc(sizeof(*view));
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
    lv_obj_add_flag(view->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(view->root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(view->root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(view->root, 0, 0);
    lv_obj_set_style_border_width(view->root, 0, 0);
    lv_obj_set_style_pad_all(view->root, 0, 0);
    lv_obj_set_style_bg_color(view->root, lv_color_hex(OTA_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(view->root, LV_OPA_COVER, 0);

    view->status_bar = system_status_bar_create(view->root);

    view->updating_content = screen_ota_create_content(view->root);
    lv_obj_set_style_pad_top(view->updating_content, 20, 0);
    lv_obj_set_style_pad_row(view->updating_content, 22, 0);

    lv_obj_t *progress_box = lv_obj_create(view->updating_content);
    screen_ota_clear_style(progress_box);
    lv_obj_remove_flag(progress_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(progress_box, 120, 120);

    view->progress_arc = lv_arc_create(progress_box);
    lv_obj_remove_flag(view->progress_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(view->progress_arc, 120, 120);
    lv_obj_center(view->progress_arc);
    lv_arc_set_range(view->progress_arc, 0, 100);
    lv_arc_set_mode(view->progress_arc, LV_ARC_MODE_REVERSE);
    lv_arc_set_rotation(view->progress_arc, 270);
    lv_arc_set_bg_angles(view->progress_arc, 0, 360);
    lv_obj_set_style_arc_width(view->progress_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(view->progress_arc, lv_color_hex(OTA_TRACK_COLOR), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(view->progress_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_width(view->progress_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(view->progress_arc, lv_color_hex(OTA_BLUE_COLOR), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(view->progress_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(view->progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(view->progress_arc, 0, LV_PART_KNOB);

    view->progress_label = screen_ota_create_label(progress_box,
                                                   "0%",
                                                   &ha_font_misan_medium_24,
                                                   OTA_PRIMARY_COLOR);
    lv_obj_center(view->progress_label);

    screen_ota_create_label(view->updating_content,
                            "Updating Firmware...",
                            &ha_font_misan_medium_24,
                            OTA_PRIMARY_COLOR);
    screen_ota_create_warning(view->updating_content, "Device restarts when done");

    view->failed_content = screen_ota_create_content(view->root);
    lv_obj_set_style_pad_top(view->failed_content, 20, 0);
    lv_obj_set_style_pad_row(view->failed_content, 14, 0);

    lv_obj_t *failed_icon = lv_image_create(view->failed_content);
    lv_image_set_src(failed_icon, &update_failed_120);
    screen_ota_create_label(view->failed_content,
                            "Update Failed",
                            &ha_font_misan_medium_24,
                            OTA_PRIMARY_COLOR);
    screen_ota_create_label(view->failed_content,
                            "Please try again",
                            &ha_font_misan_regular_20,
                            OTA_PRIMARY_COLOR);

    lv_obj_t *failed_buttons = lv_obj_create(view->failed_content);
    screen_ota_clear_style(failed_buttons);
    lv_obj_remove_flag(failed_buttons, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(failed_buttons, 236, LV_SIZE_CONTENT);
    lv_obj_set_layout(failed_buttons, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(failed_buttons, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(failed_buttons,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(failed_buttons, 0, 0);
    lv_obj_set_style_pad_row(failed_buttons, 8, 0);

    lv_obj_t *retry = screen_ota_create_button(failed_buttons, "Retry", OTA_BLUE_COLOR);
    lv_obj_add_event_cb(retry, screen_ota_retry_event_cb, LV_EVENT_CLICKED, view);
    lv_obj_t *cancel = screen_ota_create_button(failed_buttons, "Cancel", OTA_CANCEL_COLOR);
    lv_obj_add_event_cb(cancel, screen_ota_cancel_event_cb, LV_EVENT_CLICKED, view);

    view->restarting_content = screen_ota_create_content(view->root);
    lv_obj_set_style_pad_top(view->restarting_content, 20, 0);
    lv_obj_set_style_pad_row(view->restarting_content, 18, 0);

    lv_obj_t *restarting_icon = lv_image_create(view->restarting_content);
    lv_image_set_src(restarting_icon, &restarting_120);
    screen_ota_create_label(view->restarting_content,
                            "Restarting",
                            &ha_font_misan_medium_24,
                            OTA_PRIMARY_COLOR);
    screen_ota_create_label(view->restarting_content,
                            "Restarting. Please wait...",
                            &ha_font_misan_regular_20,
                            OTA_PRIMARY_COLOR);
    screen_ota_create_warning(view->restarting_content, "Do not power off");

    screen_ota_show_mode(view, SCREEN_OTA_MODE_NONE);
    return view;
}

void screen_ota_update_destroy(screen_ota_update_t *view)
{
    if (!view) {
        return;
    }
    view->root = NULL;
    lv_free(view);
}

void screen_ota_update_set_callbacks(screen_ota_update_t *view,
                                     screen_ota_update_action_cb_t retry_cb,
                                     screen_ota_update_action_cb_t cancel_cb,
                                     void *user_data)
{
    if (!view) {
        return;
    }
    view->retry_cb = retry_cb;
    view->cancel_cb = cancel_cb;
    view->callback_user_data = user_data;
}

void screen_ota_update_show_updating(screen_ota_update_t *view, uint8_t progress)
{
    if (!view) {
        return;
    }
    if (progress > 100) {
        progress = 100;
    }
    lv_arc_set_value(view->progress_arc, progress);
    lv_label_set_text_fmt(view->progress_label, "%u%%", (unsigned)progress);
    screen_ota_show_mode(view, SCREEN_OTA_MODE_UPDATING);
}

void screen_ota_update_show_failed(screen_ota_update_t *view)
{
    screen_ota_show_mode(view, SCREEN_OTA_MODE_FAILED);
}

void screen_ota_update_show_restarting(screen_ota_update_t *view)
{
    screen_ota_show_mode(view, SCREEN_OTA_MODE_RESTARTING);
}

void screen_ota_update_hide(screen_ota_update_t *view)
{
    screen_ota_show_mode(view, SCREEN_OTA_MODE_NONE);
}

bool screen_ota_update_is_visible(const screen_ota_update_t *view)
{
    return view && view->root && !lv_obj_has_flag(view->root, LV_OBJ_FLAG_HIDDEN);
}

void screen_ota_update_bring_to_front(screen_ota_update_t *view)
{
    if (!view || !view->root) {
        return;
    }

    lv_obj_move_foreground(view->root);
}

static void screen_ota_clear_style(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *screen_ota_create_content(lv_obj_t *parent)
{
    lv_obj_t *content = lv_obj_create(parent);
    screen_ota_clear_style(content);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(content, LV_PCT(100), LV_VER_RES - SYSTEM_STATUS_BAR_HEIGHT);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, SYSTEM_STATUS_BAR_HEIGHT);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    return content;
}

static lv_obj_t *screen_ota_create_label(lv_obj_t *parent,
                                         const char *text,
                                         const lv_font_t *font,
                                         uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_obj_t *screen_ota_create_warning(lv_obj_t *parent, const char *text)
{
    lv_obj_t *row = lv_obj_create(parent);
    screen_ota_clear_style(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(row, LV_SIZE_CONTENT, 32);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);

    lv_obj_t *icon = lv_obj_create(row);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(icon, 28, 28);
    lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_set_style_bg_color(icon, lv_color_hex(OTA_WARNING_COLOR), 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);

    lv_obj_t *mark = lv_label_create(icon);
    lv_label_set_text(mark, "!");
    lv_obj_set_style_text_color(mark, lv_color_hex(OTA_PRIMARY_COLOR), 0);
    lv_obj_set_style_text_font(mark, &ha_font_misan_medium_20, 0);
    lv_obj_center(mark);

    return screen_ota_create_label(row,
                                   text,
                                   &ha_font_misan_regular_20,
                                   OTA_WARNING_COLOR);
}

static lv_obj_t *screen_ota_create_button(lv_obj_t *parent,
                                          const char *text,
                                          uint32_t color)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 236, 48);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);

    lv_obj_t *label = screen_ota_create_label(button,
                                              text,
                                              &ha_font_misan_medium_24,
                                              OTA_PRIMARY_COLOR);
    lv_obj_center(label);
    return button;
}

static void screen_ota_retry_event_cb(lv_event_t *event)
{
    screen_ota_update_t *view = lv_event_get_user_data(event);
    if (view && view->retry_cb) {
        view->retry_cb(view, view->callback_user_data);
    }
}

static void screen_ota_cancel_event_cb(lv_event_t *event)
{
    screen_ota_update_t *view = lv_event_get_user_data(event);
    if (view && view->cancel_cb) {
        view->cancel_cb(view, view->callback_user_data);
    }
}

static void screen_ota_show_mode(screen_ota_update_t *view, screen_ota_mode_t mode)
{
    if (!view || !view->root) {
        return;
    }

    view->mode = mode;
    lv_obj_add_flag(view->updating_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->failed_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->restarting_content, LV_OBJ_FLAG_HIDDEN);

    if (mode == SCREEN_OTA_MODE_NONE) {
        system_status_bar_set_ota_update_visibility_override(view->status_bar,
                                                             false,
                                                             false);
        lv_obj_add_flag(view->root, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    bool show_ota_icon = mode == SCREEN_OTA_MODE_UPDATING ||
                         mode == SCREEN_OTA_MODE_FAILED;
    system_status_bar_set_ota_update_visibility_override(view->status_bar,
                                                         true,
                                                         show_ota_icon);
    lv_obj_remove_flag(view->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(view->root, -1);
    switch (mode) {
    case SCREEN_OTA_MODE_UPDATING:
        lv_obj_remove_flag(view->updating_content, LV_OBJ_FLAG_HIDDEN);
        break;
    case SCREEN_OTA_MODE_FAILED:
        lv_obj_remove_flag(view->failed_content, LV_OBJ_FLAG_HIDDEN);
        break;
    case SCREEN_OTA_MODE_RESTARTING:
        lv_obj_remove_flag(view->restarting_content, LV_OBJ_FLAG_HIDDEN);
        break;
    case SCREEN_OTA_MODE_NONE:
    default:
        break;
    }
}
