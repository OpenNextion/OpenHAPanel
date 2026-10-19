#include "screen_indicator.h"

#include <string.h>

#define SCREEN_INDICATOR_DOT_SIZE       8
#define SCREEN_INDICATOR_DOT_GAP        8
#define SCREEN_INDICATOR_BOTTOM_OFFSET  8
#define SCREEN_INDICATOR_ACTIVE_COLOR   0xE1E1E1
#define SCREEN_INDICATOR_INACTIVE_COLOR 0x999999

struct screen_indicator {
    lv_obj_t *root;
    size_t page_count;
    size_t active_index;
};

static void screen_indicator_apply_dot_style(lv_obj_t *dot, bool active)
{
    lv_obj_set_style_bg_color(
        dot,
        lv_color_hex(active ? SCREEN_INDICATOR_ACTIVE_COLOR
                            : SCREEN_INDICATOR_INACTIVE_COLOR),
        0);
}

screen_indicator_t *screen_indicator_create(lv_obj_t *parent,
                                            size_t page_count)
{
    if (!parent || page_count < 2) {
        return NULL;
    }

    screen_indicator_t *indicator = lv_malloc(sizeof(*indicator));
    if (!indicator) {
        return NULL;
    }
    memset(indicator, 0, sizeof(*indicator));
    indicator->page_count = page_count;
    indicator->active_index = page_count;

    indicator->root = lv_obj_create(parent);
    if (!indicator->root) {
        lv_free(indicator);
        return NULL;
    }

    lv_obj_remove_style_all(indicator->root);
    lv_obj_remove_flag(indicator->root,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(indicator->root, LV_SIZE_CONTENT, SCREEN_INDICATOR_DOT_SIZE);
    lv_obj_set_layout(indicator->root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(indicator->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(indicator->root,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(indicator->root, 0, 0);
    lv_obj_set_style_pad_column(indicator->root, SCREEN_INDICATOR_DOT_GAP, 0);
    lv_obj_align(indicator->root,
                 LV_ALIGN_BOTTOM_MID,
                 0,
                 -SCREEN_INDICATOR_BOTTOM_OFFSET);

    for (size_t i = 0; i < page_count; i++) {
        lv_obj_t *dot = lv_obj_create(indicator->root);
        if (!dot) {
            lv_obj_delete(indicator->root);
            lv_free(indicator);
            return NULL;
        }

        lv_obj_remove_style_all(dot);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(dot,
                        SCREEN_INDICATOR_DOT_SIZE,
                        SCREEN_INDICATOR_DOT_SIZE);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        screen_indicator_apply_dot_style(dot, false);
    }

    screen_indicator_set_active(indicator, 0);
    return indicator;
}

void screen_indicator_destroy(screen_indicator_t *indicator)
{
    if (!indicator) {
        return;
    }

    indicator->root = NULL;
    lv_free(indicator);
}

void screen_indicator_set_active(screen_indicator_t *indicator,
                                 size_t active_index)
{
    if (!indicator || !indicator->root || active_index >= indicator->page_count ||
        active_index == indicator->active_index) {
        return;
    }

    for (size_t i = 0; i < indicator->page_count; i++) {
        lv_obj_t *dot = lv_obj_get_child(indicator->root, (int32_t)i);
        if (dot) {
            screen_indicator_apply_dot_style(dot, i == active_index);
        }
    }
    indicator->active_index = active_index;
}
