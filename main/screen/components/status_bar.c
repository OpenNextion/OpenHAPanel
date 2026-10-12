#include "status_bar.h"

#include <string.h>

#define STATUS_BAR_HEIGHT 36
#define STATUS_BAR_TITLE_WIDTH 150

typedef struct {
    lv_obj_t *label;
    status_bar_icon_config_t config;
} status_bar_icon_t;

struct status_bar {
    lv_obj_t *container;
    lv_obj_t *title_label;
    lv_obj_t *time_label;
    lv_obj_t *right_container;
    lv_obj_t *icon_container;
    status_bar_icon_t *icons;
    size_t icon_count;
};

static void status_bar_apply_base_style(lv_obj_t *obj);
static void status_bar_create_layout(status_bar_t *bar, lv_obj_t *parent);
static void status_bar_clear_icons(status_bar_t *bar);
static void status_bar_apply_icon(status_bar_icon_t *target, const status_bar_icon_config_t *source);

status_bar_t *status_bar_create(lv_obj_t *parent, const status_bar_config_t *config)
{
    if (!parent) {
        return NULL;
    }

    status_bar_t *bar = lv_malloc(sizeof(status_bar_t));
    if (!bar) {
        return NULL;
    }
    memset(bar, 0, sizeof(status_bar_t));

    status_bar_create_layout(bar, parent);
    status_bar_set_title(bar, config ? config->title : NULL);
    status_bar_set_time(bar, config ? config->time_text : NULL);
    if (config && config->icons && config->icon_count > 0) {
        status_bar_set_icons(bar, config->icons, config->icon_count);
    }

    return bar;
}

void status_bar_destroy(status_bar_t *bar)
{
    if (!bar) {
        return;
    }

    status_bar_clear_icons(bar);
    lv_free(bar);
}

lv_obj_t *status_bar_get_obj(status_bar_t *bar)
{
    return bar ? bar->container : NULL;
}

void status_bar_set_title(status_bar_t *bar, const char *title)
{
    if (!bar || !bar->title_label) {
        return;
    }

    lv_label_set_text(bar->title_label, title ? title : "");
}

void status_bar_set_time(status_bar_t *bar, const char *time_text)
{
    if (!bar || !bar->time_label) {
        return;
    }

    lv_label_set_text(bar->time_label, time_text ? time_text : "");
}

void status_bar_set_icons(status_bar_t *bar, const status_bar_icon_config_t *icons, size_t icon_count)
{
    if (!bar || !bar->icon_container) {
        return;
    }

    status_bar_clear_icons(bar);
    lv_obj_clean(bar->icon_container);

    if (!icons || icon_count == 0) {
        return;
    }

    bar->icons = lv_malloc(sizeof(status_bar_icon_t) * icon_count);
    if (!bar->icons) {
        bar->icon_count = 0;
        return;
    }
    memset(bar->icons, 0, sizeof(status_bar_icon_t) * icon_count);
    bar->icon_count = icon_count;

    for (size_t i = 0; i < icon_count; i++) {
        lv_obj_t *label = lv_label_create(bar->icon_container);
        bar->icons[i].label = label;
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_pad_hor(label, 2, 0);
        status_bar_apply_icon(&bar->icons[i], &icons[i]);
    }
}

void status_bar_set_status_list(status_bar_t *bar, const status_bar_icon_config_t *icons, size_t icon_count)
{
    status_bar_set_icons(bar, icons, icon_count);
}

void status_bar_update_icon(status_bar_t *bar, size_t index, const status_bar_icon_config_t *icon)
{
    if (!bar || !bar->icons || index >= bar->icon_count || !icon) {
        return;
    }

    status_bar_apply_icon(&bar->icons[index], icon);
}

static void status_bar_apply_base_style(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void status_bar_create_layout(status_bar_t *bar, lv_obj_t *parent)
{
    lv_obj_t *container = lv_obj_create(parent);
    bar->container = container;
    status_bar_apply_base_style(container);
    lv_obj_set_size(container, LV_PCT(100), STATUS_BAR_HEIGHT);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1E222D), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    bar->title_label = lv_label_create(container);
    lv_obj_set_width(bar->title_label, STATUS_BAR_TITLE_WIDTH);
    lv_obj_set_style_pad_hor(bar->title_label, 8, 0);
    lv_obj_set_style_text_color(bar->title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(bar->title_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(bar->title_label, LV_LABEL_LONG_DOT);

    bar->right_container = lv_obj_create(container);
    lv_obj_remove_style_all(bar->right_container);
    lv_obj_set_size(bar->right_container, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_layout(bar->right_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar->right_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar->right_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(bar->right_container, 8, 0);
    lv_obj_set_style_pad_column(bar->right_container, 6, 0);

    bar->icon_container = lv_obj_create(bar->right_container);
    lv_obj_remove_style_all(bar->icon_container);
    lv_obj_set_size(bar->icon_container, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_layout(bar->icon_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar->icon_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar->icon_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar->icon_container, 4, 0);

    bar->time_label = lv_label_create(bar->right_container);
    lv_obj_set_style_text_color(bar->time_label, lv_color_hex(0xC8CDD8), 0);
    lv_obj_set_style_text_font(bar->time_label, &lv_font_montserrat_14, 0);
}

static void status_bar_clear_icons(status_bar_t *bar)
{
    if (!bar || !bar->icons) {
        return;
    }

    lv_free(bar->icons);
    bar->icons = NULL;
    bar->icon_count = 0;
}

static void status_bar_apply_icon(status_bar_icon_t *target, const status_bar_icon_config_t *source)
{
    if (!target || !target->label || !source) {
        return;
    }

    target->config = *source;
    lv_label_set_text(target->label, source->symbol ? source->symbol : "");
    lv_obj_set_style_text_color(target->label, source->color, 0);

    if (source->visible) {
        lv_obj_remove_flag(target->label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(target->label, LV_OBJ_FLAG_HIDDEN);
    }
}
