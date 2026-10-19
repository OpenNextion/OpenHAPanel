#include "system_status_bar.h"

#include <string.h>
#include <time.h>

#include "../../assets/ha_ui_assets.h"
#include "sys_time_api.h"

#define SYSTEM_STATUS_BAR_PRIMARY_COLOR 0xFFFFFF
#define SYSTEM_STATUS_BAR_REFRESH_MS     1000
#define SYSTEM_STATUS_BAR_TITLE_WIDTH    168

struct system_status_bar {
    lv_obj_t *container;
    lv_obj_t *title_label;
    lv_obj_t *ota_update_icon;
    lv_obj_t *hass_icon;
    lv_obj_t *wifi_icon;
    lv_obj_t *time_label;
    lv_timer_t *refresh_timer;
    bool ota_update_override_enabled;
    bool ota_update_override_visible;
    struct system_status_bar *next;
};

static system_status_bar_t *s_status_bars;
static bool s_ota_update_visible;
static bool s_hass_connected_visible;
static bool s_wifi_sta_connected_visible;

static void system_status_bar_delete_event_cb(lv_event_t *event);
static void system_status_bar_refresh_ota_update_icon(system_status_bar_t *bar);
static void system_status_bar_refresh_time(system_status_bar_t *bar);
static void system_status_bar_timer_cb(lv_timer_t *timer);

system_status_bar_t *system_status_bar_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    system_status_bar_t *bar = lv_malloc(sizeof(*bar));
    if (!bar) {
        return NULL;
    }
    memset(bar, 0, sizeof(*bar));

    bar->container = lv_obj_create(parent);
    if (!bar->container) {
        lv_free(bar);
        return NULL;
    }

    lv_obj_remove_style_all(bar->container);
    lv_obj_remove_flag(bar->container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bar->container, LV_PCT(100), SYSTEM_STATUS_BAR_HEIGHT);
    lv_obj_align(bar->container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_layout(bar->container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar->container,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bar->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar->container, 0, 0);
    lv_obj_set_style_pad_all(bar->container, 0, 0);
    lv_obj_set_style_pad_left(bar->container, 12, 0);
    lv_obj_set_style_pad_right(bar->container, 12, 0);
    lv_obj_add_event_cb(bar->container,
                        system_status_bar_delete_event_cb,
                        LV_EVENT_DELETE,
                        bar);

    bar->title_label = lv_label_create(bar->container);
    lv_obj_set_size(bar->title_label,
                    SYSTEM_STATUS_BAR_TITLE_WIDTH,
                    lv_font_get_line_height(&ha_font_misan_regular_16));
    lv_label_set_long_mode(bar->title_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_color(bar->title_label,
                                lv_color_hex(SYSTEM_STATUS_BAR_PRIMARY_COLOR),
                                0);
    lv_obj_set_style_text_font(bar->title_label, &ha_font_misan_regular_16, 0);
    system_status_bar_set_title(bar, "");

    lv_obj_t *right = lv_obj_create(bar->container);
    lv_obj_remove_style_all(right);
    lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_layout(right, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 8, 0);

    bar->ota_update_icon = lv_image_create(right);
    lv_image_set_src(bar->ota_update_icon, &cloud_update_22);
    lv_obj_remove_flag(bar->ota_update_icon, LV_OBJ_FLAG_CLICKABLE);
    system_status_bar_refresh_ota_update_icon(bar);

    bar->hass_icon = lv_image_create(right);
    lv_image_set_src(bar->hass_icon, &hass_24x24);
    lv_obj_remove_flag(bar->hass_icon, LV_OBJ_FLAG_CLICKABLE);
    if (!s_hass_connected_visible) {
        lv_obj_add_flag(bar->hass_icon, LV_OBJ_FLAG_HIDDEN);
    }

    bar->wifi_icon = lv_image_create(right);
    lv_image_set_src(bar->wifi_icon, &wifi_24x24);
    lv_obj_remove_flag(bar->wifi_icon, LV_OBJ_FLAG_CLICKABLE);
    if (!s_wifi_sta_connected_visible) {
        lv_obj_add_flag(bar->wifi_icon, LV_OBJ_FLAG_HIDDEN);
    }

    bar->time_label = lv_label_create(right);
    lv_obj_set_style_text_color(bar->time_label,
                                lv_color_hex(SYSTEM_STATUS_BAR_PRIMARY_COLOR),
                                0);
    lv_obj_set_style_text_font(bar->time_label, &ha_font_misan_medium_24, 0);
    system_status_bar_refresh_time(bar);
    bar->refresh_timer = lv_timer_create(system_status_bar_timer_cb,
                                         SYSTEM_STATUS_BAR_REFRESH_MS,
                                         bar);

    bar->next = s_status_bars;
    s_status_bars = bar;

    return bar;
}

lv_obj_t *system_status_bar_get_obj(system_status_bar_t *bar)
{
    return bar ? bar->container : NULL;
}

void system_status_bar_set_title(system_status_bar_t *bar, const char *title)
{
    if (!bar || !bar->title_label) {
        return;
    }

    lv_label_set_text(bar->title_label, title ? title : "");
}

void system_status_bar_set_time(system_status_bar_t *bar, const char *time_text)
{
    if (!bar || !bar->time_label) {
        return;
    }

    const char *text = time_text && time_text[0] ? time_text : "--:--";
    const char *current_text = lv_label_get_text(bar->time_label);
    if (!current_text || strcmp(current_text, text) != 0) {
        lv_label_set_text(bar->time_label, text);
    }
}

void system_status_bar_set_ota_update_visible(bool visible)
{
    s_ota_update_visible = visible;
    for (system_status_bar_t *bar = s_status_bars; bar; bar = bar->next) {
        system_status_bar_refresh_ota_update_icon(bar);
    }
}

void system_status_bar_set_ota_update_visibility_override(system_status_bar_t *bar,
                                                          bool enabled,
                                                          bool visible)
{
    if (!bar) {
        return;
    }

    bar->ota_update_override_enabled = enabled;
    bar->ota_update_override_visible = visible;
    system_status_bar_refresh_ota_update_icon(bar);
}

void system_status_bar_set_hass_connected_visible(bool visible)
{
    s_hass_connected_visible = visible;
    for (system_status_bar_t *bar = s_status_bars; bar; bar = bar->next) {
        if (!bar->hass_icon) {
            continue;
        }
        if (visible) {
            lv_obj_remove_flag(bar->hass_icon, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(bar->hass_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void system_status_bar_set_wifi_sta_connected_visible(bool visible)
{
    s_wifi_sta_connected_visible = visible;
    for (system_status_bar_t *bar = s_status_bars; bar; bar = bar->next) {
        if (!bar->wifi_icon) {
            continue;
        }
        if (visible) {
            lv_obj_remove_flag(bar->wifi_icon, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(bar->wifi_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void system_status_bar_refresh_ota_update_icon(system_status_bar_t *bar)
{
    if (!bar || !bar->ota_update_icon) {
        return;
    }

    bool visible = bar->ota_update_override_enabled
                       ? bar->ota_update_override_visible
                       : s_ota_update_visible;
    if (visible) {
        lv_obj_remove_flag(bar->ota_update_icon, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(bar->ota_update_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

static void system_status_bar_delete_event_cb(lv_event_t *event)
{
    system_status_bar_t *bar = lv_event_get_user_data(event);
    if (!bar) {
        return;
    }

    if (bar->refresh_timer) {
        lv_timer_delete(bar->refresh_timer);
        bar->refresh_timer = NULL;
    }

    system_status_bar_t **cursor = &s_status_bars;
    while (*cursor && *cursor != bar) {
        cursor = &(*cursor)->next;
    }
    if (*cursor == bar) {
        *cursor = bar->next;
    }

    bar->container = NULL;
    bar->title_label = NULL;
    bar->ota_update_icon = NULL;
    bar->hass_icon = NULL;
    bar->wifi_icon = NULL;
    bar->time_label = NULL;
    bar->next = NULL;
    lv_free(bar);
}

static void system_status_bar_refresh_time(system_status_bar_t *bar)
{
    time_t now = 0;
    time(&now);
    if (now < 1577836800) {
        system_status_bar_set_time(bar, "--:--");
        return;
    }

    struct tm *local_time = sys_time_get_local_time();
    if (!local_time) {
        system_status_bar_set_time(bar, "--:--");
        return;
    }

    char time_text[6] = {0};
    if (strftime(time_text, sizeof(time_text), "%H:%M", local_time) == 0) {
        system_status_bar_set_time(bar, "--:--");
        return;
    }
    system_status_bar_set_time(bar, time_text);
}

static void system_status_bar_timer_cb(lv_timer_t *timer)
{
    system_status_bar_t *bar = lv_timer_get_user_data(timer);
    system_status_bar_refresh_time(bar);
}
