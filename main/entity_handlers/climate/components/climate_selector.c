#include "climate_selector.h"

#include <stdio.h>
#include <string.h>

#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"

#define EFFECT_MAX_OPTIONS 16
#define EFFECT_NAME_MAX    48

#define EFFECT_BUTTON_W 140
#define EFFECT_BUTTON_H 56
#define EFFECT_ROW_H    40
#define EFFECT_INSET    0
#define EFFECT_MARGIN   8
#define EFFECT_MENU_RADIUS 12

#define EFFECT_BUTTON_COLOR      0x1F1F1F
#define EFFECT_BUTTON_PRESSED    0x303030
#define EFFECT_MENU_COLOR        0x1E2228
#define EFFECT_MENU_BORDER_COLOR 0x44474C
#define EFFECT_TEXT_COLOR        0xFFFFFF
#define EFFECT_SELECTED_BG_COLOR 0x34373D

typedef struct climate_selector_context climate_selector_context_t;

typedef struct {
    climate_selector_context_t *context;
    size_t index;
} climate_selector_row_binding_t;

struct climate_selector_context {
    lv_obj_t *root;
    lv_obj_t *icon_label;
    lv_obj_t *title_label;
    lv_obj_t *value_label;
    lv_obj_t *popup_mask;
    lv_obj_t *menu;
    lv_obj_t *rows[EFFECT_MAX_OPTIONS];
    climate_selector_row_binding_t bindings[EFFECT_MAX_OPTIONS];
    char options[EFFECT_MAX_OPTIONS][EFFECT_NAME_MAX];
    const char *option_icons[EFFECT_MAX_OPTIONS];
    size_t option_count;
    size_t selected;
    bool disabled;
    climate_selector_change_cb_t callback;
    void *user_data;
};

static climate_selector_context_t *effect_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static void display_option(const char *source, char *output, size_t size)
{
    if (!output || !size) return;
    snprintf(output, size, "%s", source ? source : "-");
    for (char *cursor = output; *cursor; ++cursor) {
        if (*cursor == '_') *cursor = ' ';
    }
    if (output[0] >= 'a' && output[0] <= 'z') output[0] -= 'a' - 'A';
}

static void close_popup(climate_selector_context_t *context)
{
    if (!context || !context->popup_mask) {
        return;
    }

    lv_obj_t *mask = context->popup_mask;
    context->popup_mask = NULL;
    context->menu = NULL;
    memset(context->rows, 0, sizeof(context->rows));
    if (lv_obj_is_valid(mask)) {
        lv_obj_delete(mask);
    }
}

static void refresh_trigger(climate_selector_context_t *context)
{
    if (!context || !context->icon_label || !context->title_label ||
        !context->value_label) {
        return;
    }

    char label[EFFECT_NAME_MAX];
    display_option(context->option_count > 0 && context->selected < context->option_count
                       ? context->options[context->selected] : "-", label, sizeof(label));
    lv_label_set_text(context->value_label, label);
    if (context->option_count > 0 && context->selected < context->option_count) {
        const char *icon = context->option_icons[context->selected];
        if (icon && icon[0]) climate_selector_set_icon(context->root, icon);
    }
}

static void refresh_rows(climate_selector_context_t *context)
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
                                        lv_color_hex(EFFECT_TEXT_COLOR),
                                        0);
        }
    }
}

static void popup_delete_cb(lv_event_t *event)
{
    climate_selector_context_t *context = lv_event_get_user_data(event);
    if (context && context->popup_mask == lv_event_get_target_obj(event)) {
        context->popup_mask = NULL;
        context->menu = NULL;
        memset(context->rows, 0, sizeof(context->rows));
    }
}

static void popup_clicked_cb(lv_event_t *event)
{
    climate_selector_context_t *context = lv_event_get_user_data(event);
    if (context && lv_event_get_target_obj(event) ==
                       lv_event_get_current_target_obj(event)) {
        close_popup(context);
    }
}

static void row_clicked_cb(lv_event_t *event)
{
    climate_selector_row_binding_t *binding = lv_event_get_user_data(event);
    if (!binding || !binding->context) {
        return;
    }
    climate_selector_context_t *context = binding->context;
    if (context->disabled || binding->index >= context->option_count) {
        return;
    }

    const size_t selected = binding->index;
    char effect[EFFECT_NAME_MAX];
    snprintf(effect, sizeof(effect), "%s", context->options[selected]);
    context->selected = selected;
    refresh_trigger(context);

    climate_selector_change_cb_t callback = context->callback;
    void *user_data = context->user_data;
    close_popup(context);
    if (callback) {
        callback(selected, effect, user_data);
    }
}

static lv_obj_t *create_menu_row(climate_selector_context_t *context,
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
                   EFFECT_INSET,
                   EFFECT_INSET + (int32_t)index * EFFECT_ROW_H);
    lv_obj_set_size(row, width, EFFECT_ROW_H);
    const bool edge_row = index == 0 || index + 1 == context->option_count;
    lv_obj_set_style_radius(row, edge_row ? EFFECT_MENU_RADIUS : 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row,
                              lv_color_hex(EFFECT_SELECTED_BG_COLOR),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row,
                            LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(row,
                        row_clicked_cb,
                        LV_EVENT_CLICKED,
                        &context->bindings[index]);

    lv_obj_t *label = lv_label_create(row);
    char text[EFFECT_NAME_MAX];
    display_option(context->options[index], text, sizeof(text));
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    bool has_icon = context->option_icons[index] && context->option_icons[index][0];
    lv_obj_set_pos(label, has_icon ? 30 : 12, 10);
    lv_obj_set_size(label, LV_MAX(1, width - (has_icon ? 38 : 20)), 20);
    lv_obj_set_style_text_font(label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(EFFECT_TEXT_COLOR), 0);
    if (has_icon) {
        lv_obj_t *icon = lv_label_create(row);
        if (icon) {
            lv_label_set_text(icon, context->option_icons[index]);
            lv_obj_set_pos(icon, 6, 10);
            lv_obj_set_size(icon, 20, 20);
            lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
            lv_obj_set_style_text_color(icon, lv_color_hex(EFFECT_TEXT_COLOR), 0);
        }
    }
    return row;
}

static void open_popup(climate_selector_context_t *context)
{
    if (!context || context->disabled || context->option_count == 0 ||
        context->popup_mask) {
        return;
    }

    lv_display_t *display = lv_obj_get_display(context->root);
    if (!display) {
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

    const int32_t natural_height =
        EFFECT_INSET * 2 + (int32_t)context->option_count * EFFECT_ROW_H;
    const int32_t max_height =
        LV_MAX(1, screen_height - EFFECT_MARGIN * 2);
    const int32_t menu_height = LV_MIN(natural_height, max_height);

    lv_obj_update_layout(context->root);
    lv_area_t root_coords;
    lv_obj_get_coords(context->root, &root_coords);
    int32_t menu_width = LV_MIN(LV_MAX(133, lv_area_get_width(&root_coords)),
                                LV_MAX(1, screen_width - EFFECT_MARGIN * 2));
    int32_t menu_x = root_coords.x1 +
                     (lv_area_get_width(&root_coords) - menu_width) / 2;
    menu_x = LV_CLAMP(EFFECT_MARGIN,
                      menu_x,
                      screen_width - menu_width - EFFECT_MARGIN);
    int32_t menu_y = root_coords.y1 - menu_height;
    menu_y = LV_CLAMP(EFFECT_MARGIN,
                      menu_y,
                      screen_height - menu_height - EFFECT_MARGIN);

    lv_obj_t *menu = lv_obj_create(mask);
    if (!menu) {
        close_popup(context);
        return;
    }
    context->menu = menu;
    lv_obj_remove_style_all(menu);
    lv_obj_set_pos(menu, menu_x, menu_y);
    lv_obj_set_size(menu, menu_width, menu_height);
    lv_obj_set_style_radius(menu, EFFECT_MENU_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(menu,
                              lv_color_hex(EFFECT_MENU_COLOR),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        menu,
        lv_color_hex(EFFECT_MENU_BORDER_COLOR),
        LV_PART_MAIN);
    lv_obj_set_style_border_opa(menu, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(menu, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(menu, 40, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(menu, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(menu, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(menu, LV_SCROLLBAR_MODE_AUTO);

    for (size_t i = 0; i < context->option_count; i++) {
        context->rows[i] = create_menu_row(context, menu, i, menu_width);
    }
    refresh_rows(context);
    lv_obj_update_layout(menu);
    if (context->selected < context->option_count &&
        context->rows[context->selected]) {
        lv_obj_scroll_to_view(context->rows[context->selected], LV_ANIM_OFF);
    }
}

static void root_event_cb(lv_event_t *event)
{
    climate_selector_context_t *context = lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        close_popup(context);
        lv_free(context);
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

lv_obj_t *climate_selector_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    climate_selector_context_t *context = lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }

    lv_obj_t *root = lv_button_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    context->root = root;
    lv_obj_set_user_data(root, context);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, EFFECT_BUTTON_W, EFFECT_BUTTON_H);
    lv_obj_set_style_radius(root, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root,
                              lv_color_hex(EFFECT_BUTTON_COLOR),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root,
                              lv_color_hex(EFFECT_BUTTON_PRESSED),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(root,
                            LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root, root_event_cb, LV_EVENT_ALL, context);

    context->icon_label = lv_label_create(root);
    lv_label_set_text(context->icon_label, MDI_SYMBOL_CREATION);
    lv_obj_set_style_text_font(context->icon_label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(context->icon_label,
                                lv_color_hex(EFFECT_TEXT_COLOR),
                                0);
    lv_obj_align(context->icon_label, LV_ALIGN_LEFT_MID, 8, 0);

    context->title_label = lv_label_create(root);
    lv_label_set_text(context->title_label, "Mode");
    lv_label_set_long_mode(context->title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(context->title_label, 32, 7);
    lv_obj_set_size(context->title_label, 95, 18);
    lv_obj_set_style_text_font(context->title_label,
                               &ha_font_misan_regular_14,
                               0);
    lv_obj_set_style_text_color(context->title_label,
                                lv_color_hex(EFFECT_TEXT_COLOR),
                                0);

    context->value_label = lv_label_create(root);
    lv_label_set_text(context->value_label, "");
    lv_label_set_long_mode(context->value_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(context->value_label, 32, 27);
    lv_obj_set_size(context->value_label, 95, 20);
    lv_obj_set_style_text_font(context->value_label,
                               &ha_font_misan_regular_14,
                               0);
    lv_obj_set_style_text_color(context->value_label,
                                lv_color_hex(EFFECT_TEXT_COLOR),
                                0);
    refresh_trigger(context);
    return root;
}

bool climate_selector_set_options(lv_obj_t *obj,
                                  const char *const *options,
                                  const char *const *icons, size_t count)
{
    climate_selector_context_t *context = effect_context(obj);
    if (!context || !options || count == 0) {
        return false;
    }

    count = LV_MIN(count, EFFECT_MAX_OPTIONS);
    bool unchanged = context->option_count == count;
    for (size_t i = 0; unchanged && i < count; ++i) {
        if (strncmp(context->options[i], options[i] ? options[i] : "",
                    EFFECT_NAME_MAX - 1) != 0 ||
            context->option_icons[i] != (icons ? icons[i] : NULL))
            unchanged = false;
    }
    if (unchanged) return true;
    close_popup(context);
    memset(context->options, 0, sizeof(context->options));
    memset(context->option_icons, 0, sizeof(context->option_icons));
    for (size_t i = 0; i < count; i++) {
        snprintf(context->options[i],
                 sizeof(context->options[i]),
                 "%s",
                 options[i] ? options[i] : "");
        context->option_icons[i] = icons ? icons[i] : NULL;
    }
    context->option_count = count;
    context->selected = LV_MIN(context->selected, count - 1);
    refresh_trigger(context);
    return true;
}

void climate_selector_set_selected(lv_obj_t *obj, size_t index)
{
    climate_selector_context_t *context = effect_context(obj);
    if (!context || context->option_count == 0) {
        return;
    }
    context->selected = LV_MIN(index, context->option_count - 1);
    refresh_trigger(context);
    refresh_rows(context);
}

void climate_selector_set_disabled(lv_obj_t *obj, bool disabled)
{
    climate_selector_context_t *context = effect_context(obj);
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
    lv_obj_set_style_opa(obj, disabled ? LV_OPA_50 : LV_OPA_COVER, 0);
}

void climate_selector_set_change_cb(lv_obj_t *obj,
                                         climate_selector_change_cb_t callback,
                                         void *user_data)
{
    climate_selector_context_t *context = effect_context(obj);
    if (!context) {
        return;
    }
    context->callback = callback;
    context->user_data = user_data;
}

void climate_selector_set_title(lv_obj_t *obj, const char *title)
{
    climate_selector_context_t *context = effect_context(obj);
    if (context) lv_label_set_text(context->title_label, title ? title : "");
}

void climate_selector_set_icon(lv_obj_t *obj, const char *icon)
{
    climate_selector_context_t *context = effect_context(obj);
    if (context) lv_label_set_text(context->icon_label, icon ? icon : "");
}

void climate_selector_set_accent(lv_obj_t *obj, lv_color_t color)
{
    climate_selector_context_t *context = effect_context(obj);
    if (context) lv_obj_set_style_text_color(context->icon_label, color, 0);
}

void climate_selector_set_content_layout(lv_obj_t *obj, bool wide)
{
    climate_selector_context_t *context = effect_context(obj);
    if (!context) return;

    lv_obj_update_layout(obj);
    int32_t width = lv_obj_get_width(obj);
    int32_t icon_x = wide ? (width - 62) / 2 : 8;
    int32_t label_x = wide ? icon_x + 26 : 32;
    int32_t label_width = LV_MAX(1, width - label_x - 6);
    lv_obj_align(context->icon_label, LV_ALIGN_LEFT_MID, icon_x, 0);
    lv_obj_set_pos(context->title_label, label_x, 7);
    lv_obj_set_width(context->title_label, label_width);
    lv_obj_set_pos(context->value_label, label_x, 27);
    lv_obj_set_width(context->value_label, label_width);
}
