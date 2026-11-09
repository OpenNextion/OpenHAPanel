#include "ha_control_select_menu.h"
#include "ui_i18n.h"
#include "ha_ui_font.h"
#include "i18n.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "screen_theme.h"
#include <lvgl_private.h>

#define SELECT_MENU_MAX_OPTIONS HA_CONTROL_SELECT_MENU_MAX_OPTIONS
#define SELECT_MENU_NAME_MAX    HA_CONTROL_SELECT_MENU_MAX_VALUE_LEN
#define SELECT_MENU_LABEL_MAX   96
#define SELECT_MENU_SYMBOL_MAX  32

#define SELECT_MENU_BUTTON_H 56
#define SELECT_MENU_ROW_H    40
#define SELECT_MENU_INSET    0
#define SELECT_MENU_MARGIN   8
#define SELECT_MENU_MENU_RADIUS 12
#define SELECT_MENU_SCROLLBAR_INSET 8

#define SELECT_MENU_BUTTON_COLOR      0x999999
#define SELECT_MENU_BUTTON_OPA        71
#define SELECT_MENU_BUTTON_PRESSED    0x303030
#define SELECT_MENU_MENU_COLOR        0x1E2228
#define SELECT_MENU_MENU_BORDER_COLOR 0x44474C
#define SELECT_MENU_TEXT_COLOR        0xFFFFFF
#define SELECT_MENU_SELECTED_BG_COLOR 0x34373D
#define SELECT_MENU_LIGHT_TEXT_COLOR        0x333333
#define SELECT_MENU_LIGHT_BUTTON_COLOR      0x999999
#define SELECT_MENU_LIGHT_PRESSED_COLOR     0xC8CDD5
#define SELECT_MENU_LIGHT_MENU_COLOR        0xFFFFFF
#define SELECT_MENU_LIGHT_BORDER_COLOR      0xD9DEE7
#define SELECT_MENU_LIGHT_SCROLLBAR_COLOR   0xBDBDBD

typedef struct ha_control_select_menu_context ha_control_select_menu_context_t;

typedef struct {
    ha_control_select_menu_context_t *context;
    size_t index;
} ha_control_select_menu_row_binding_t;

struct ha_control_select_menu_context {
    lv_obj_t *root;
    lv_obj_t *icon_label;
    lv_obj_t *title_label;
    lv_obj_t *value_label;
    lv_obj_t *popup_mask;
    lv_obj_t *menu;
    lv_obj_t *rows[SELECT_MENU_MAX_OPTIONS];
    ha_control_select_menu_row_binding_t bindings[SELECT_MENU_MAX_OPTIONS];
    char options[SELECT_MENU_MAX_OPTIONS][SELECT_MENU_NAME_MAX];
    char labels[SELECT_MENU_MAX_OPTIONS][SELECT_MENU_LABEL_MAX];
    char option_icons[SELECT_MENU_MAX_OPTIONS][SELECT_MENU_SYMBOL_MAX];
    size_t option_count;
    size_t selected;
    bool disabled;
    ha_control_select_menu_colors_t colors[2];
    ha_control_select_menu_change_cb_t callback;
    void *user_data;
};

static const ha_control_select_menu_colors_t default_colors[2] = {
    {
        .button_color = SELECT_MENU_BUTTON_COLOR,
        .button_opa = SELECT_MENU_BUTTON_OPA,
        .pressed_color = SELECT_MENU_BUTTON_PRESSED,
        .text_color = SELECT_MENU_TEXT_COLOR,
        .menu_color = SELECT_MENU_MENU_COLOR,
        .menu_border_color = SELECT_MENU_MENU_BORDER_COLOR,
        .scrollbar_color = SELECT_MENU_MENU_BORDER_COLOR,
        .row_pressed_color = SELECT_MENU_SELECTED_BG_COLOR,
    },
    {
        .button_color = SELECT_MENU_LIGHT_BUTTON_COLOR,
        .button_opa = SELECT_MENU_BUTTON_OPA,
        .pressed_color = SELECT_MENU_LIGHT_PRESSED_COLOR,
        .text_color = SELECT_MENU_LIGHT_TEXT_COLOR,
        .menu_color = SELECT_MENU_LIGHT_MENU_COLOR,
        .menu_border_color = SELECT_MENU_LIGHT_BORDER_COLOR,
        .scrollbar_color = SELECT_MENU_LIGHT_SCROLLBAR_COLOR,
        .row_pressed_color = SELECT_MENU_LIGHT_PRESSED_COLOR,
    },
};

static const ha_control_select_menu_colors_t *selector_colors(
    const ha_control_select_menu_context_t *context)
{
    return &context->colors[screen_theme_get() == SCREEN_THEME_LIGHT ? 1 : 0];
}

static uint32_t selector_text_color(const ha_control_select_menu_context_t *context)
{
    return selector_colors(context)->text_color;
}

static uint32_t selector_button_color(const ha_control_select_menu_context_t *context)
{
    return selector_colors(context)->button_color;
}

static lv_opa_t selector_button_opa(const ha_control_select_menu_context_t *context)
{
    return selector_colors(context)->button_opa;
}

static uint32_t selector_pressed_color(const ha_control_select_menu_context_t *context)
{
    return selector_colors(context)->pressed_color;
}

static ha_control_select_menu_context_t *menu_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static void display_option(ha_control_select_menu_context_t *context,
                           size_t index, char *output, size_t size)
{
    i18n_display_copy(output, size,
                      index < context->option_count ? context->labels[index] : "-");
}

static void close_popup(ha_control_select_menu_context_t *context)
{
    if (!context || !context->popup_mask) {
        return;
    }

    lv_obj_set_style_bg_color(context->root,
                              lv_color_hex(selector_button_color(context)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(context->root,
                            selector_button_opa(context), LV_PART_MAIN);

    lv_obj_t *mask = context->popup_mask;
    context->popup_mask = NULL;
    context->menu = NULL;
    memset(context->rows, 0, sizeof(context->rows));
    if (lv_obj_is_valid(mask)) {
        lv_obj_delete(mask);
    }
}

static void reposition_labels(ha_control_select_menu_context_t *context)
{
    if (!context || !context->root || !context->title_label ||
        !context->value_label || !context->icon_label) {
        return;
    }

    lv_obj_update_layout(context->root);
    int32_t root_w = lv_obj_get_width(context->root);

    const char *title_text = lv_label_get_text(context->title_label);
    const char *value_text = lv_label_get_text(context->value_label);

    lv_text_attributes_t text_attr;
    lv_text_attributes_init(&text_attr);

    int32_t title_w = title_text
        ? lv_text_get_width(title_text, strlen(title_text),
                            ha_ui_font_regular(14), &text_attr)
        : 0;
    int32_t value_w = value_text
        ? lv_text_get_width(value_text, strlen(value_text),
                            ha_ui_font_regular(14), &text_attr)
        : 0;
    int32_t max_label_w = LV_MAX(title_w, value_w);

    const int32_t icon_w = 24;
    const int32_t gap = 4;
    const int32_t right_pad = 6;
    int32_t avail_w = root_w - icon_w - gap - right_pad;
    if (avail_w < 0) avail_w = 0;
    if (max_label_w > avail_w) max_label_w = avail_w;

    lv_obj_set_width(context->title_label, max_label_w);
    lv_obj_set_width(context->value_label, max_label_w);

    int32_t group_w = icon_w + gap + max_label_w;
    int32_t left_offset = (root_w - group_w) / 2;
    if (left_offset < 0) left_offset = 0;

    lv_obj_set_x(context->icon_label, left_offset);
    lv_obj_set_x(context->title_label, left_offset + icon_w + gap);
    lv_obj_set_x(context->value_label, left_offset + icon_w + gap);
}

static void refresh_trigger(ha_control_select_menu_context_t *context)
{
    if (!context || !context->icon_label || !context->title_label ||
        !context->value_label) {
        return;
    }

    char label[SELECT_MENU_LABEL_MAX];
    display_option(context, context->selected, label, sizeof(label));
    ui_i18n_label_set_text(context->value_label, label);
    reposition_labels(context);
}

static void refresh_rows(ha_control_select_menu_context_t *context)
{
    if (!context) {
        return;
    }
    for (size_t i = 0; i < context->option_count; i++) {
        lv_obj_t *row = context->rows[i];
        if (!row) {
            continue;
        }
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_t *label = lv_obj_get_child(row, 0);
        if (label) {
            lv_obj_set_style_text_color(label,
                                        lv_color_hex(selector_text_color(context)),
                                        0);
        }
    }
}

static void popup_delete_cb(lv_event_t *event)
{
    ha_control_select_menu_context_t *context = lv_event_get_user_data(event);
    if (context && context->popup_mask == lv_event_get_target_obj(event)) {
        context->popup_mask = NULL;
        context->menu = NULL;
        memset(context->rows, 0, sizeof(context->rows));
    }
}

static void popup_clicked_cb(lv_event_t *event)
{
    ha_control_select_menu_context_t *context = lv_event_get_user_data(event);
    if (context && lv_event_get_target_obj(event) ==
                       lv_event_get_current_target_obj(event)) {
        close_popup(context);
    }
}

static void row_clicked_cb(lv_event_t *event)
{
    ha_control_select_menu_row_binding_t *binding = lv_event_get_user_data(event);
    if (!binding || !binding->context) {
        return;
    }
    ha_control_select_menu_context_t *context = binding->context;
    if (context->disabled || binding->index >= context->option_count) {
        return;
    }

    const size_t selected = binding->index;
    char effect[SELECT_MENU_NAME_MAX];
    snprintf(effect, sizeof(effect), "%s", context->options[selected]);
    context->selected = selected;
    refresh_trigger(context);

    ha_control_select_menu_change_cb_t callback = context->callback;
    void *user_data = context->user_data;
    close_popup(context);
    if (callback) {
        callback(effect, user_data);
    }
}

static lv_obj_t *create_menu_row(ha_control_select_menu_context_t *context,
                                 lv_obj_t *menu,
                                 size_t index, int32_t width)
{
    lv_obj_t *row = lv_button_create(menu);
    if (!row) {
        return NULL;
    }
    context->bindings[index].context = context;
    context->bindings[index].index = index;
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row,
                   SELECT_MENU_INSET,
                   SELECT_MENU_INSET + (int32_t)index * SELECT_MENU_ROW_H);
    lv_obj_set_size(row, width, SELECT_MENU_ROW_H);
    const bool edge_row = index == 0 || index + 1 == context->option_count;
    lv_obj_set_style_radius(row, edge_row ? SELECT_MENU_MENU_RADIUS : 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row,
                              lv_color_hex(selector_colors(context)->row_pressed_color),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row,
                            LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(row,
                        row_clicked_cb,
                        LV_EVENT_CLICKED,
                        &context->bindings[index]);

    lv_obj_t *label = lv_label_create(row);
    char text[SELECT_MENU_LABEL_MAX];
    display_option(context, index, text, sizeof(text));
    ui_i18n_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    bool has_icon = context->option_icons[index][0] != '\0';
    lv_obj_set_pos(label, has_icon ? 30 : 12, 10);
    lv_obj_set_size(label, LV_MAX(1, width - (has_icon ? 38 : 20)), 20);
    lv_obj_set_style_text_font(label, ha_ui_font_regular(16), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(selector_text_color(context)), 0);
    if (has_icon) {
        lv_obj_t *icon = lv_label_create(row);
        if (icon) {
            ui_i18n_label_set_text(icon, context->option_icons[index]);
            lv_obj_set_pos(icon, 6, 10);
            lv_obj_set_size(icon, 20, 20);
            lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
            lv_obj_set_style_text_color(icon,
                                        lv_color_hex(selector_text_color(context)),
                                        0);
        }
    }
    return row;
}

static void open_popup(ha_control_select_menu_context_t *context)
{
    if (!context || context->disabled || context->option_count == 0 ||
        context->popup_mask) {
        return;
    }

    lv_obj_set_style_bg_color(context->root,
                              lv_color_hex(selector_pressed_color(context)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(context->root, LV_OPA_COVER, LV_PART_MAIN);

    lv_display_t *display = lv_obj_get_display(context->root);
    if (!display) {
        lv_obj_set_style_bg_color(context->root,
                                  lv_color_hex(selector_button_color(context)),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(context->root,
                                selector_button_opa(context), LV_PART_MAIN);
        return;
    }
    const int32_t screen_width =
        lv_display_get_horizontal_resolution(display);
    const int32_t screen_height =
        lv_display_get_vertical_resolution(display);

    lv_obj_t *mask = lv_obj_create(lv_layer_top());
    if (!mask) {
        return;
    }
    context->popup_mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, screen_width, screen_height);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mask,
                        popup_clicked_cb,
                        LV_EVENT_CLICKED,
                        context);
    lv_obj_add_event_cb(mask,
                        popup_delete_cb,
                        LV_EVENT_DELETE,
                        context);

    lv_obj_update_layout(context->root);
    lv_area_t root_coords;
    lv_obj_get_coords(context->root, &root_coords);
    const int32_t natural_height =
        SELECT_MENU_INSET * 2 + (int32_t)context->option_count * SELECT_MENU_ROW_H;
    const int32_t available_above = LV_MAX(0, root_coords.y1 - SELECT_MENU_MARGIN);
    const int32_t available_below = LV_MAX(0, screen_height - root_coords.y2 - SELECT_MENU_MARGIN);
    const bool open_below = available_below >= natural_height ||
                            available_below > available_above;
    const int32_t available_height = LV_MAX(1, open_below ? available_below : available_above);
    const int32_t menu_height = LV_MIN(natural_height, available_height);
    const bool needs_scroll = natural_height > available_height;
    int32_t max_text_w = 0;
    lv_text_attributes_t text_attr;
    lv_text_attributes_init(&text_attr);
    for (size_t i = 0; i < context->option_count; i++) {
        char text[SELECT_MENU_LABEL_MAX];
        display_option(context, i, text, sizeof(text));
        bool has_icon = context->option_icons[i][0] != '\0';
        int32_t tw = lv_text_get_width(text, strlen(text),
                                       ha_ui_font_regular(16), &text_attr);
        int32_t row_w = tw + (has_icon ? 38 : 20);
        if (row_w > max_text_w) max_text_w = row_w;
    }
    int32_t menu_width = LV_MAX(lv_area_get_width(&root_coords), max_text_w);
    menu_width = LV_MIN(menu_width,
                        LV_MAX(1, screen_width - SELECT_MENU_MARGIN * 2));
    int32_t menu_x = root_coords.x1 +
                     (lv_area_get_width(&root_coords) - menu_width) / 2;
    menu_x = LV_CLAMP(SELECT_MENU_MARGIN,
                      menu_x,
                      screen_width - menu_width - SELECT_MENU_MARGIN);
    const int32_t menu_y = open_below ? root_coords.y2 + 1 : root_coords.y1 - menu_height;

    lv_obj_t *menu = lv_obj_create(mask);
    if (!menu) {
        close_popup(context);
        return;
    }
    context->menu = menu;
    lv_obj_remove_style_all(menu);
    lv_obj_set_pos(menu, menu_x, menu_y);
    lv_obj_set_size(menu, menu_width, menu_height);
    lv_obj_set_style_radius(menu, SELECT_MENU_MENU_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(menu,
                              lv_color_hex(selector_colors(context)->menu_color),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        menu,
        lv_color_hex(selector_colors(context)->menu_border_color),
        LV_PART_MAIN);
    lv_obj_set_style_border_opa(menu, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(menu, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(menu, 40, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(menu, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu, 0, LV_PART_MAIN);
    lv_obj_remove_flag(menu, LV_OBJ_FLAG_SCROLL_ELASTIC);
    if (needs_scroll) {
        lv_obj_add_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(menu, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(menu, LV_SCROLLBAR_MODE_AUTO);
    } else {
        lv_obj_remove_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(menu, LV_SCROLLBAR_MODE_OFF);
    }
    lv_obj_set_style_width(menu, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(menu, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_top(menu, SELECT_MENU_SCROLLBAR_INSET, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_bottom(menu, SELECT_MENU_SCROLLBAR_INSET, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(menu, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(menu,
                              lv_color_hex(selector_colors(context)->scrollbar_color),
                              LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, LV_PART_SCROLLBAR);

    for (size_t i = 0; i < context->option_count; i++) {
        context->rows[i] = create_menu_row(context, menu, i, menu_width);
    }
    refresh_rows(context);
    lv_obj_update_layout(menu);
    if (needs_scroll && context->selected < context->option_count &&
        context->rows[context->selected]) {
        lv_obj_scroll_to_view(context->rows[context->selected], LV_ANIM_OFF);
    }
}

static void root_event_cb(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
    ha_control_select_menu_context_t *context = lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        close_popup(context);
        lv_free(context);
        return;
    }
    if (code == LV_EVENT_SIZE_CHANGED) {
        reposition_labels(context);
        return;
    }
    if (code != LV_EVENT_CLICKED || context->disabled) {
        return;
    }
    if (context->popup_mask) {
        close_popup(context);
    }
    else {
        open_popup(context);
    }
}

lv_obj_t *ha_control_select_menu_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    ha_control_select_menu_context_t *context = lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }
    memcpy(context->colors, default_colors, sizeof(default_colors));

    lv_obj_t *root = lv_button_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    context->root = root;
    lv_obj_set_user_data(root, context);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), SELECT_MENU_BUTTON_H);
    lv_obj_set_style_radius(root, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root,
                              lv_color_hex(selector_button_color(context)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, selector_button_opa(context), LV_PART_MAIN);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(root, root_event_cb, LV_EVENT_ALL, context);

    context->icon_label = lv_label_create(root);
    ui_i18n_label_set_text(context->icon_label, MDI_SYMBOL_CREATION);
    lv_obj_set_style_text_font(context->icon_label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(context->icon_label,
                                lv_color_hex(selector_text_color(context)),
                                0);
    lv_obj_align(context->icon_label, LV_ALIGN_LEFT_MID, 8, 0);

    context->title_label = lv_label_create(root);
    ui_i18n_label_set_text(context->title_label, "");
    lv_label_set_long_mode(context->title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(context->title_label, 34, 7);
    lv_obj_set_style_text_font(context->title_label,
                               ha_ui_font_regular(14),
                               0);
    lv_obj_set_style_text_color(context->title_label,
                                lv_color_hex(selector_text_color(context)),
                                0);

    context->value_label = lv_label_create(root);
    ui_i18n_label_set_text(context->value_label, "");
    lv_label_set_long_mode(context->value_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(context->value_label, 34, 27);
    lv_obj_set_style_text_font(context->value_label,
                               ha_ui_font_regular(14),
                               0);
    lv_obj_set_style_text_color(context->value_label,
                                lv_color_hex(selector_text_color(context)),
                                0);
    refresh_trigger(context);
    return root;
}

bool ha_control_select_menu_set_options(lv_obj_t *obj,
                                        const ha_control_select_menu_option_t *options,
                                        size_t count)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context || (count && !options)) return false;
    count = LV_MIN(count, SELECT_MENU_MAX_OPTIONS);
    bool unchanged = context->option_count == count;
    for (size_t i = 0; unchanged && i < count; ++i) {
        if (strncmp(context->options[i], options[i].value ? options[i].value : "",
                    sizeof(context->options[i]) - 1) != 0 ||
            strncmp(context->labels[i], options[i].label ? options[i].label : "",
                    sizeof(context->labels[i]) - 1) != 0 ||
            strncmp(context->option_icons[i],
                    options[i].symbol ? options[i].symbol : "",
                    sizeof(context->option_icons[i]) - 1) != 0)
            unchanged = false;
    }
    if (unchanged) return true;
    close_popup(context);
    memset(context->options, 0, sizeof(context->options));
    memset(context->labels, 0, sizeof(context->labels));
    memset(context->option_icons, 0, sizeof(context->option_icons));
    for (size_t i = 0; i < count; i++) {
        lv_strlcpy(context->options[i], options[i].value ? options[i].value : "",
                   sizeof(context->options[i]));
        lv_strlcpy(context->labels[i], options[i].label ? options[i].label :
                   context->options[i], sizeof(context->labels[i]));
        lv_strlcpy(context->option_icons[i],
                   options[i].symbol ? options[i].symbol : "",
                   sizeof(context->option_icons[i]));
    }
    context->option_count = count;
    if (context->selected >= count) context->selected = SIZE_MAX;
    refresh_trigger(context);
    return true;
}

void ha_control_select_menu_set_value(lv_obj_t *obj, const char *value)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context) return;
    size_t selected = SIZE_MAX;
    if (value) {
        for (size_t i = 0; i < context->option_count; i++) {
            if (strcmp(context->options[i], value) == 0) {
                selected = i;
                break;
            }
        }
    }
    if (context->selected == selected) return;
    context->selected = selected;
    refresh_trigger(context);
    refresh_rows(context);
}

size_t ha_control_select_menu_get_option_count(lv_obj_t *obj)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    return context ? context->option_count : 0;
}

void ha_control_select_menu_set_disabled(lv_obj_t *obj, bool disabled)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context) {
        return;
    }
    context->disabled = disabled;
    if (disabled) {
        close_popup(context);
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
    else {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    }
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
}

void ha_control_select_menu_set_change_cb(lv_obj_t *obj,
                                         ha_control_select_menu_change_cb_t callback,
                                         void *user_data)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context) {
        return;
    }
    context->callback = callback;
    context->user_data = user_data;
}

void ha_control_select_menu_set_title(lv_obj_t *obj, const char *title)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context) return;
    ui_i18n_label_set_text(context->title_label, title ? title : "");
    reposition_labels(context);
}

void ha_control_select_menu_set_icon(lv_obj_t *obj, const char *icon)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (context) {
        ui_i18n_label_set_text(context->icon_label, icon ? icon : "");
    }
}

bool ha_control_select_menu_get_colors(lv_obj_t *obj, bool light,
                                       ha_control_select_menu_colors_t *colors)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context || !colors) return false;
    *colors = context->colors[light ? 1 : 0];
    return true;
}

void ha_control_select_menu_set_colors(lv_obj_t *obj, bool light,
                                       const ha_control_select_menu_colors_t *colors)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context || !colors) return;
    context->colors[light ? 1 : 0] = *colors;
    ha_control_select_menu_apply_theme(obj);
}

void ha_control_select_menu_apply_theme(lv_obj_t *obj)
{
    ha_control_select_menu_context_t *context = menu_context(obj);
    if (!context) return;
    lv_obj_set_style_bg_color(obj,
                              context->popup_mask
                                  ? lv_color_hex(selector_pressed_color(context))
                                  : lv_color_hex(selector_button_color(context)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj,
                            context->popup_mask ? LV_OPA_COVER
                                                : selector_button_opa(context),
                            LV_PART_MAIN);
    lv_obj_set_style_text_color(context->title_label,
                                lv_color_hex(selector_text_color(context)), 0);
    lv_obj_set_style_text_color(context->value_label,
                                lv_color_hex(selector_text_color(context)), 0);
    lv_obj_set_style_text_color(context->icon_label,
                                lv_color_hex(selector_text_color(context)), 0);
    if (context->menu) {
        lv_obj_set_style_bg_color(context->menu,
                                  lv_color_hex(selector_colors(context)->menu_color),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_color(context->menu,
                                      lv_color_hex(selector_colors(context)->menu_border_color),
                                      LV_PART_MAIN);
        lv_obj_set_style_bg_color(context->menu,
                                  lv_color_hex(selector_colors(context)->scrollbar_color),
                                  LV_PART_SCROLLBAR);
        for (size_t i = 0; i < context->option_count; i++) {
            lv_obj_t *row = context->rows[i];
            if (!row) continue;
            lv_obj_set_style_bg_color(row,
                                      lv_color_hex(selector_colors(context)->row_pressed_color),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_t *icon = lv_obj_get_child(row, 1);
            if (icon) {
                lv_obj_set_style_text_color(icon,
                                            lv_color_hex(selector_text_color(context)), 0);
            }
        }
    }
    refresh_rows(context);
}
