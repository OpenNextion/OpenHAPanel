#include "notification.h"

#include <stdbool.h>
#include <string.h>

#include "lvgl.h"
#include "../../assets/ha_ui_assets.h"

#define NOTIFICATION_DIALOG_WIDTH      LV_PCT(100)
#define NOTIFICATION_ICON_SIZE         32
#define NOTIFICATION_DIALOG_PADDING    20
#define NOTIFICATION_CONTENT_GAP       6
#define NOTIFICATION_SWIPE_AREA_HEIGHT 44

struct notification_t {
    lv_obj_t *overlay;
    lv_timer_t *timer;
    bool closing;
    notification_close_reason_t close_reason;
    notification_dialog_click_cb_t dialog_click_cb;
    void *dialog_click_user_data;
    notification_swipe_down_cb_t swipe_down_cb;
    void *swipe_down_user_data;
    notification_closed_cb_t closed_cb;
    void *closed_user_data;
};

static lv_color_t notification_level_color(notification_level_t level);
static void notification_apply_clear_style(lv_obj_t *obj);
static lv_obj_t *notification_create_icon(lv_obj_t *parent,
                                          const void *src,
                                          int32_t size,
                                          lv_color_t color);
static void notification_begin_close(notification_t *notification,
                                     notification_close_reason_t reason);
static void notification_dialog_click_event_cb(lv_event_t *event);
static void notification_enable_dialog_click_area(lv_obj_t *obj);
static void notification_overlay_click_event_cb(lv_event_t *event);
static void notification_overlay_gesture_event_cb(lv_event_t *event);
static void notification_timer_cb(lv_timer_t *timer);
static void notification_overlay_delete_event_cb(lv_event_t *event);

notification_t *notification_show(const notification_config_t *config)
{
    if (!config || !config->title) {
        return NULL;
    }

    notification_t *notification = lv_malloc(sizeof(notification_t));
    if (!notification) {
        return NULL;
    }
    memset(notification, 0, sizeof(notification_t));
    notification->dialog_click_cb = config->dialog_click_cb;
    notification->dialog_click_user_data = config->dialog_click_user_data;
    notification->swipe_down_cb = config->swipe_down_cb;
    notification->swipe_down_user_data = config->swipe_down_user_data;
    notification->closed_cb = config->closed_cb;
    notification->closed_user_data = config->closed_user_data;
    notification->close_reason = NOTIFICATION_CLOSE_PARENT_DELETED;

    lv_obj_t *overlay = lv_obj_create(lv_layer_sys());
    notification->overlay = overlay;

    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay,
                            config->transparent_overlay ? LV_OPA_TRANSP : LV_OPA_70,
                            0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    if (config->closeable) {
        lv_obj_add_event_cb(overlay, notification_overlay_click_event_cb, LV_EVENT_CLICKED, notification);
    }
    lv_obj_add_event_cb(overlay, notification_overlay_delete_event_cb, LV_EVENT_DELETE, notification);

    lv_color_t level_color = config->accent_color
                                 ? lv_color_hex(config->accent_color)
                                 : notification_level_color(config->level);
    lv_color_t dialog_background_color = config->dialog_background_color
                                             ? lv_color_hex(config->dialog_background_color)
                                             : lv_color_hex(0x303030);
    lv_color_t description_color = config->description_color
                                       ? lv_color_hex(config->description_color)
                                       : lv_color_hex(0xB8B8B8);
    int32_t icon_size = config->icon_size
                            ? (int32_t)config->icon_size
                            : NOTIFICATION_ICON_SIZE;
    lv_obj_t *dialog = lv_obj_create(overlay);
    lv_obj_set_width(dialog, NOTIFICATION_DIALOG_WIDTH);
    lv_obj_set_height(dialog, LV_SIZE_CONTENT);
    lv_obj_align(dialog, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(dialog, LV_OBJ_FLAG_CLICKABLE);
    if (config->dialog_click_cb) {
        lv_obj_add_event_cb(dialog, notification_dialog_click_event_cb, LV_EVENT_CLICKED, notification);
    }
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(dialog, dialog_background_color, 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dialog, level_color, 0);
    lv_obj_set_style_border_width(dialog, 0, 0);
    lv_obj_set_style_radius(dialog, 0, 0);
    lv_obj_set_style_pad_hor(dialog, NOTIFICATION_DIALOG_PADDING, 0);
    lv_obj_set_style_pad_ver(dialog, NOTIFICATION_DIALOG_PADDING, 0);
    lv_obj_set_style_pad_row(dialog, NOTIFICATION_CONTENT_GAP, 0);
    lv_obj_set_layout(dialog, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dialog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *header_ctn = lv_obj_create(dialog);
    notification_apply_clear_style(header_ctn);
    lv_obj_set_size(header_ctn, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header_ctn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_ctn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_ctn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(header_ctn, 0, 0);
    lv_obj_set_style_pad_column(header_ctn, 8, 0);
    lv_obj_set_flag(header_ctn, LV_OBJ_FLAG_SCROLLABLE, false);

    if (config->icon_src) {
        lv_obj_t *header_icon_box = lv_obj_create(header_ctn);
        lv_obj_remove_style_all(header_icon_box);
        lv_obj_set_size(header_icon_box, icon_size, icon_size);
        lv_obj_set_flex_grow(header_icon_box, 0);
        lv_obj_set_style_bg_color(header_icon_box, level_color, 0);
        lv_obj_set_style_bg_opa(header_icon_box,
                                config->icon_includes_background
                                    ? LV_OPA_TRANSP
                                    : LV_OPA_COVER,
                                0);
        lv_obj_set_style_radius(header_icon_box, 7, 0);
        lv_obj_clear_flag(header_icon_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        notification_create_icon(header_icon_box,
                                 config->icon_src,
                                 icon_size,
                                 lv_color_white());
    }

    lv_obj_t *header_title_ctn = lv_obj_create(header_ctn);
    notification_apply_clear_style(header_title_ctn);
    lv_obj_set_height(header_title_ctn, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(header_title_ctn, 1);
    lv_obj_set_layout(header_title_ctn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_title_ctn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(header_title_ctn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(header_title_ctn, 0, 0);
    lv_obj_set_style_pad_column(header_title_ctn, 5, 0);

    lv_obj_t *header_title_label = lv_label_create(header_title_ctn);
    lv_label_set_text(header_title_label, config->title);
    lv_obj_set_style_text_color(header_title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(header_title_label, &ha_font_misan_medium_20, 0);
    lv_obj_set_size(header_title_label, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_long_mode(header_title_label, LV_LABEL_LONG_WRAP);

    if (config->status_text && config->status_text[0] != '\0') {
        lv_obj_t *status_parent = config->status_in_header
                                      ? header_title_ctn
                                      : dialog;
        lv_obj_t *status_ctn = lv_obj_create(status_parent);
        notification_apply_clear_style(status_ctn);
        lv_obj_set_size(status_ctn, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(status_ctn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(status_ctn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_ctn,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(status_ctn, 6, 0);

        if (config->status_icon_src) {
            notification_create_icon(status_ctn,
                                     config->status_icon_src,
                                     20,
                                     level_color);
        }

        lv_obj_t *status_label = lv_label_create(status_ctn);
        lv_label_set_text(status_label, config->status_text);
        lv_obj_set_style_text_color(status_label, level_color, 0);
        lv_obj_set_style_text_font(status_label, &ha_font_misan_regular_14, 0);
        lv_obj_set_flex_grow(status_label, 1);
        lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    }

    if (config->description && config->description[0] != '\0') {
        lv_obj_t *description_label = lv_label_create(dialog);
        lv_label_set_text(description_label, config->description);
        lv_obj_set_style_text_color(description_label, description_color, 0);
        lv_obj_set_style_text_font(description_label, &ha_font_misan_regular_14, 0);
        lv_obj_set_size(description_label, LV_PCT(100), LV_SIZE_CONTENT);
        lv_label_set_long_mode(description_label, LV_LABEL_LONG_WRAP);
    }

    if (config->tips && config->tips[0] != '\0') {
        lv_obj_t *split_line = lv_obj_create(dialog);
        notification_apply_clear_style(split_line);
        lv_obj_set_size(split_line, LV_PCT(100), 2);
        lv_obj_set_style_bg_color(split_line, lv_color_hex(0x30393f), 0);
        lv_obj_set_style_bg_opa(split_line, LV_OPA_COVER, 0);

        lv_obj_t *tips_ctn = lv_obj_create(dialog);
        notification_apply_clear_style(tips_ctn);
        lv_obj_set_size(tips_ctn, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(tips_ctn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(tips_ctn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tips_ctn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(tips_ctn, 0, 0);
        lv_obj_set_style_pad_column(tips_ctn, 5, 0);

        lv_obj_t *tips_icon = lv_label_create(tips_ctn);
        notification_apply_clear_style(tips_icon);
        lv_obj_set_size(tips_icon, 20, 20);
        lv_label_set_text(tips_icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_color(tips_icon, lv_color_white(), 0);

        lv_obj_t *tips_label = lv_label_create(tips_ctn);
        lv_label_set_text(tips_label, config->tips);
        lv_obj_set_flex_grow(tips_label, 1);
        lv_obj_set_style_text_color(tips_label, lv_color_hex(0xB8B8B8), 0);
        lv_obj_set_style_text_font(tips_label, &ha_font_misan_regular_20, 0);
        lv_label_set_long_mode(tips_label, LV_LABEL_LONG_MODE_WRAP);
    }

    if (config->durations > 0) {
        notification->timer = lv_timer_create(notification_timer_cb, config->durations, notification);
        if (notification->timer) {
            lv_timer_set_repeat_count(notification->timer, 1);
        }
    }

    if (config->dialog_click_cb) {
        notification_enable_dialog_click_area(dialog);
    }

    if (config->swipe_down_cb) {
        lv_obj_t *swipe_area = lv_obj_create(overlay);
        lv_obj_remove_style_all(swipe_area);
        lv_obj_remove_flag(swipe_area,
                           LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(swipe_area, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(swipe_area,
                        LV_PCT(100),
                        NOTIFICATION_SWIPE_AREA_HEIGHT);
        lv_obj_align(swipe_area, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa(swipe_area, LV_OPA_TRANSP, 0);
        lv_obj_add_event_cb(swipe_area,
                            notification_overlay_gesture_event_cb,
                            LV_EVENT_GESTURE,
                            notification);
        lv_obj_move_foreground(swipe_area);
    }

    lv_obj_move_foreground(overlay);
    return notification;
}

void notification_close(notification_t *notification)
{
    notification_begin_close(notification, NOTIFICATION_CLOSE_PROGRAMMATIC);
}

static lv_color_t notification_level_color(notification_level_t level)
{
    switch (level) {
    case NOTIFICATION_LEVEL_SUCCESS:
        return lv_color_hex(0x4CAF50);
    case NOTIFICATION_LEVEL_WARNING:
        return lv_color_hex(0xFF9800);
    case NOTIFICATION_LEVEL_ERROR:
        return lv_color_hex(0xF2777A);
    case NOTIFICATION_LEVEL_INACTIVE:
        return lv_color_hex(0xB8B8B8);
    case NOTIFICATION_LEVEL_INFO:
    default:
        return lv_color_hex(0x65B7FF);
    }
}

static void notification_apply_clear_style(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *notification_create_icon(lv_obj_t *parent,
                                          const void *src,
                                          int32_t size,
                                          lv_color_t color)
{
    if (!parent || !src) {
        return NULL;
    }

    lv_obj_t *icon = lv_image_create(parent);
    lv_image_set_src(icon, src);
    lv_obj_set_size(icon, size, size);
    lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_style_text_color(icon, color, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(icon);
    return icon;
}

static void notification_begin_close(notification_t *notification,
                                     notification_close_reason_t reason)
{
    if (!notification) {
        return;
    }
    if (notification->closing) {
        return;
    }
    notification->closing = true;
    notification->close_reason = reason;

    if (notification->timer) {
        lv_timer_delete(notification->timer);
        notification->timer = NULL;
    }
    if (notification->overlay) {
        lv_obj_delete_async(notification->overlay);
        return;
    }

    lv_free(notification);
}

static void notification_dialog_click_event_cb(lv_event_t *event)
{
    notification_t *notification = lv_event_get_user_data(event);
    if (!notification || !notification->dialog_click_cb) {
        return;
    }

    notification->dialog_click_cb(notification, notification->dialog_click_user_data);
}

static void notification_enable_dialog_click_area(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);

    uint32_t child_count = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < child_count; i++) {
        notification_enable_dialog_click_area(lv_obj_get_child(obj, i));
    }
}

static void notification_overlay_click_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) != lv_event_get_current_target(event)) {
        return;
    }

    notification_t *notification = lv_event_get_user_data(event);
    notification_begin_close(notification, NOTIFICATION_CLOSE_USER);
}

static void notification_overlay_gesture_event_cb(lv_event_t *event)
{
    notification_t *notification = lv_event_get_user_data(event);
    lv_indev_t *indev = lv_indev_active();
    if (!notification || !notification->swipe_down_cb || !indev ||
        lv_indev_get_gesture_dir(indev) != LV_DIR_BOTTOM) {
        return;
    }

    notification->swipe_down_cb(notification,
                                notification->swipe_down_user_data);
}

static void notification_timer_cb(lv_timer_t *timer)
{
    notification_t *notification = lv_timer_get_user_data(timer);
    if (!notification) {
        return;
    }

    notification->timer = NULL;
    if (notification->overlay) {
        notification->closing = true;
        notification->close_reason = NOTIFICATION_CLOSE_TIMEOUT;
        lv_obj_delete_async(notification->overlay);
    }
}

static void notification_overlay_delete_event_cb(lv_event_t *event)
{
    notification_t *notification = lv_event_get_user_data(event);
    if (!notification) {
        return;
    }

    if (notification->timer) {
        lv_timer_delete(notification->timer);
        notification->timer = NULL;
    }
    notification->overlay = NULL;
    if (notification->closed_cb) {
        notification->closed_cb(notification,
                                notification->close_reason,
                                notification->closed_user_data);
    }
    lv_free(notification);
}
