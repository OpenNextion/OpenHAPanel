#include "light_effect_selector.h"

#include <stdio.h>
#include <string.h>

#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"

#define LIGHT_EFFECT_MAX_OPTIONS 16
#define LIGHT_EFFECT_NAME_MAX    48
#define LIGHT_EFFECT_BUTTON_W    140
#define LIGHT_EFFECT_BUTTON_H    56
#define LIGHT_EFFECT_ROW_H       40
#define LIGHT_EFFECT_MARGIN      8
#define LIGHT_EFFECT_RADIUS      12

#define LIGHT_EFFECT_ICON_X          8
#define LIGHT_EFFECT_ICON_SIZE       20
#define LIGHT_EFFECT_TEXT_X          36
#define LIGHT_EFFECT_TEXT_W          91
#define LIGHT_EFFECT_TITLE_Y         19
#define LIGHT_EFFECT_TITLE_ACTIVE_Y  7
#define LIGHT_EFFECT_VALUE_Y         27

#define LIGHT_EFFECT_BUTTON_COLOR  0x1F1F1F
#define LIGHT_EFFECT_BUTTON_PRESS  0x303030
#define LIGHT_EFFECT_MENU_COLOR    0x1E2228
#define LIGHT_EFFECT_MENU_BORDER   0x44474C
#define LIGHT_EFFECT_TEXT_COLOR    0xFFFFFF
#define LIGHT_EFFECT_ROW_PRESS     0x34373D

typedef struct light_effect_selector_context light_effect_selector_context_t;

typedef struct {
    light_effect_selector_context_t *context;
    size_t index;
} light_effect_row_binding_t;

struct light_effect_selector_context {
    lv_obj_t *root;
    lv_obj_t *icon_label;
    lv_obj_t *title_label;
    lv_obj_t *value_label;
    lv_obj_t *popup_mask;
    lv_obj_t *menu;
    lv_obj_t *rows[LIGHT_EFFECT_MAX_OPTIONS];
    light_effect_row_binding_t bindings[LIGHT_EFFECT_MAX_OPTIONS];
    char options[LIGHT_EFFECT_MAX_OPTIONS][LIGHT_EFFECT_NAME_MAX];
    size_t option_count;
    size_t selected;
    bool power_on;
    bool disabled;
    light_effect_change_cb_t callback;
    void *user_data;
};

static light_effect_selector_context_t *effect_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static bool effect_is_none(const char *effect)
{
    return !effect || effect[0] == '\0' || strcmp(effect, "None") == 0 ||
           strcmp(effect, "none") == 0;
}

static void display_effect(const char *source, char *output, size_t size)
{
    if (!output || size == 0) {
        return;
    }
    snprintf(output, size, "%s", source ? source : "-");
    for (char *cursor = output; *cursor; ++cursor) {
        if (*cursor == '_') {
            *cursor = ' ';
        }
    }
    if (output[0] >= 'a' && output[0] <= 'z') {
        output[0] -= 'a' - 'A';
    }
}

static void close_popup(light_effect_selector_context_t *context)
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

static void refresh_trigger(light_effect_selector_context_t *context)
{
    if (!context) {
        return;
    }
    const bool has_effect = context->power_on &&
                            context->option_count > 0 &&
                            context->selected < context->option_count &&
                            !effect_is_none(context->options[context->selected]);
    if (has_effect) {
        char label[LIGHT_EFFECT_NAME_MAX];
        display_effect(context->options[context->selected], label, sizeof(label));
        lv_label_set_text(context->icon_label, MDI_SYMBOL_CIRCLE_MEDIUM);
        lv_obj_set_y(context->title_label, LIGHT_EFFECT_TITLE_ACTIVE_Y);
        lv_label_set_text(context->value_label, label);
        lv_obj_remove_flag(context->value_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Preserve the light-off state: no active effect value is displayed. */
    lv_label_set_text(context->icon_label, MDI_SYMBOL_CREATION);
    lv_obj_set_y(context->title_label, LIGHT_EFFECT_TITLE_Y);
    lv_label_set_text(context->value_label, "");
    lv_obj_add_flag(context->value_label, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_rows(light_effect_selector_context_t *context)
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
                                        lv_color_hex(LIGHT_EFFECT_TEXT_COLOR),
                                        0);
        }
    }
}

static void popup_delete_cb(lv_event_t *event)
{
    light_effect_selector_context_t *context = lv_event_get_user_data(event);
    if (context && context->popup_mask == lv_event_get_target_obj(event)) {
        context->popup_mask = NULL;
        context->menu = NULL;
        memset(context->rows, 0, sizeof(context->rows));
    }
}

static void popup_clicked_cb(lv_event_t *event)
{
    light_effect_selector_context_t *context = lv_event_get_user_data(event);
    if (context && lv_event_get_target_obj(event) ==
                       lv_event_get_current_target_obj(event)) {
        close_popup(context);
    }
}

static void row_clicked_cb(lv_event_t *event)
{
    light_effect_row_binding_t *binding = lv_event_get_user_data(event);
    if (!binding || !binding->context) {
        return;
    }
    light_effect_selector_context_t *context = binding->context;
    if (context->disabled || binding->index >= context->option_count) {
        return;
    }
    const size_t selected = binding->index;
    char effect[LIGHT_EFFECT_NAME_MAX];
    snprintf(effect, sizeof(effect), "%s", context->options[selected]);
    context->selected = selected;
    refresh_trigger(context);

    light_effect_change_cb_t callback = context->callback;
    void *user_data = context->user_data;
    close_popup(context);
    if (callback) {
        callback(selected, effect, user_data);
    }
}

static lv_obj_t *create_menu_row(light_effect_selector_context_t *context,
                                 lv_obj_t *menu,
                                 size_t index,
                                 int32_t width)
{
    lv_obj_t *row = lv_button_create(menu);
    if (!row) {
        return NULL;
    }
    context->bindings[index].context = context;
    context->bindings[index].index = index;
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, 0, (int32_t)index * LIGHT_EFFECT_ROW_H);
    lv_obj_set_size(row, width, LIGHT_EFFECT_ROW_H);
    const bool edge = index == 0 || index + 1 == context->option_count;
    lv_obj_set_style_radius(row, edge ? LIGHT_EFFECT_RADIUS : 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row,
                              lv_color_hex(LIGHT_EFFECT_ROW_PRESS),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row,
                            LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(row,
                        row_clicked_cb,
                        LV_EVENT_CLICKED,
                        &context->bindings[index]);

    lv_obj_t *label = lv_label_create(row);
    char text[LIGHT_EFFECT_NAME_MAX];
    display_effect(context->options[index], text, sizeof(text));
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(label, 30, 10);
    lv_obj_set_size(label, LV_MAX(1, width - 38), 20);
    lv_obj_set_style_text_font(label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(LIGHT_EFFECT_TEXT_COLOR), 0);

    lv_obj_t *icon = lv_label_create(row);
    if (icon) {
        lv_label_set_text(icon, MDI_SYMBOL_CIRCLE_MEDIUM);
        lv_obj_set_pos(icon, 6, 10);
        lv_obj_set_size(icon, 20, 20);
        lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(LIGHT_EFFECT_TEXT_COLOR), 0);
    }
    return row;
}

static void open_popup(light_effect_selector_context_t *context)
{
    if (!context || context->disabled || context->option_count == 0 ||
        context->popup_mask) {
        return;
    }
    lv_display_t *display = lv_obj_get_display(context->root);
    if (!display) {
        return;
    }
    const int32_t screen_width = lv_display_get_horizontal_resolution(display);
    const int32_t screen_height = lv_display_get_vertical_resolution(display);

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
    lv_obj_add_event_cb(mask, popup_clicked_cb, LV_EVENT_CLICKED, context);
    lv_obj_add_event_cb(mask, popup_delete_cb, LV_EVENT_DELETE, context);

    const int32_t natural_height =
        (int32_t)context->option_count * LIGHT_EFFECT_ROW_H;
    const int32_t max_height = LV_MAX(1, screen_height - 2 * LIGHT_EFFECT_MARGIN);
    const int32_t menu_height = LV_MIN(natural_height, max_height);

    lv_obj_update_layout(context->root);
    lv_area_t root_coords;
    lv_obj_get_coords(context->root, &root_coords);
    int32_t menu_width = LV_MIN(LV_MAX(133, lv_area_get_width(&root_coords)),
                                LV_MAX(1, screen_width - 2 * LIGHT_EFFECT_MARGIN));
    int32_t menu_x = root_coords.x1 +
                     (lv_area_get_width(&root_coords) - menu_width) / 2;
    menu_x = LV_CLAMP(LIGHT_EFFECT_MARGIN,
                      menu_x,
                      screen_width - menu_width - LIGHT_EFFECT_MARGIN);
    int32_t menu_y = root_coords.y1 - menu_height;
    menu_y = LV_CLAMP(LIGHT_EFFECT_MARGIN,
                      menu_y,
                      screen_height - menu_height - LIGHT_EFFECT_MARGIN);

    lv_obj_t *menu = lv_obj_create(mask);
    if (!menu) {
        close_popup(context);
        return;
    }
    context->menu = menu;
    lv_obj_remove_style_all(menu);
    lv_obj_set_pos(menu, menu_x, menu_y);
    lv_obj_set_size(menu, menu_width, menu_height);
    lv_obj_set_style_radius(menu, LIGHT_EFFECT_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(menu,
                              lv_color_hex(LIGHT_EFFECT_MENU_COLOR),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(menu,
                                  lv_color_hex(LIGHT_EFFECT_MENU_BORDER),
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
    light_effect_selector_context_t *context = lv_event_get_user_data(event);
    if (!context) {
        return;
    }
    if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        close_popup(context);
        lv_free(context);
        return;
    }
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || context->disabled) {
        return;
    }
    if (context->popup_mask) {
        close_popup(context);
    }
    else {
        open_popup(context);
    }
}

lv_obj_t *light_effect_selector_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_effect_selector_context_t *context = lv_calloc(1, sizeof(*context));
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
    lv_obj_set_size(root, LIGHT_EFFECT_BUTTON_W, LIGHT_EFFECT_BUTTON_H);
    lv_obj_set_style_radius(root, LIGHT_EFFECT_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root,
                              lv_color_hex(LIGHT_EFFECT_BUTTON_COLOR),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root,
                              lv_color_hex(LIGHT_EFFECT_BUTTON_PRESS),
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
                                lv_color_hex(LIGHT_EFFECT_TEXT_COLOR),
                                0);
    lv_obj_set_size(context->icon_label,
                    LIGHT_EFFECT_ICON_SIZE,
                    LIGHT_EFFECT_ICON_SIZE);
    lv_obj_set_style_text_align(context->icon_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(context->icon_label,
                 LV_ALIGN_LEFT_MID,
                 LIGHT_EFFECT_ICON_X,
                 0);

    context->title_label = lv_label_create(root);
    lv_label_set_text(context->title_label, "Effect");
    lv_obj_set_pos(context->title_label,
                   LIGHT_EFFECT_TEXT_X,
                   LIGHT_EFFECT_TITLE_Y);
    lv_obj_set_size(context->title_label, LIGHT_EFFECT_TEXT_W, 18);
    lv_obj_set_style_text_font(context->title_label,
                               &ha_font_misan_regular_12,
                               0);
    lv_obj_set_style_text_color(context->title_label,
                                lv_color_hex(LIGHT_EFFECT_TEXT_COLOR),
                                0);

    context->value_label = lv_label_create(root);
    lv_label_set_text(context->value_label, "");
    lv_label_set_long_mode(context->value_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(context->value_label,
                   LIGHT_EFFECT_TEXT_X,
                   LIGHT_EFFECT_VALUE_Y);
    lv_obj_set_size(context->value_label, LIGHT_EFFECT_TEXT_W, 20);
    lv_obj_set_style_text_font(context->value_label,
                               &ha_font_misan_regular_16,
                               0);
    lv_obj_set_style_text_color(context->value_label,
                                lv_color_hex(LIGHT_EFFECT_TEXT_COLOR),
                                0);
    lv_obj_add_flag(context->value_label, LV_OBJ_FLAG_HIDDEN);
    return root;
}

bool light_effect_selector_set_options(lv_obj_t *obj,
                                       const char *const *options,
                                       size_t count)
{
    light_effect_selector_context_t *context = effect_context(obj);
    if (!context || !options || count == 0) {
        return false;
    }
    count = LV_MIN(count, LIGHT_EFFECT_MAX_OPTIONS);
    bool unchanged = context->option_count == count;
    for (size_t i = 0; unchanged && i < count; i++) {
        if (strncmp(context->options[i],
                    options[i] ? options[i] : "",
                    LIGHT_EFFECT_NAME_MAX - 1) != 0) {
            unchanged = false;
        }
    }
    if (unchanged) {
        return true;
    }
    close_popup(context);
    memset(context->options, 0, sizeof(context->options));
    for (size_t i = 0; i < count; i++) {
        snprintf(context->options[i],
                 sizeof(context->options[i]),
                 "%s",
                 options[i] ? options[i] : "");
    }
    context->option_count = count;
    context->selected = LV_MIN(context->selected, count - 1);
    refresh_trigger(context);
    return true;
}

void light_effect_selector_set_selected(lv_obj_t *obj, size_t index)
{
    light_effect_selector_context_t *context = effect_context(obj);
    if (!context || context->option_count == 0) {
        return;
    }
    context->selected = LV_MIN(index, context->option_count - 1);
    refresh_trigger(context);
    refresh_rows(context);
}

void light_effect_selector_set_power(lv_obj_t *obj, bool on)
{
    light_effect_selector_context_t *context = effect_context(obj);
    if (!context) {
        return;
    }
    context->power_on = on;
    refresh_trigger(context);
}

void light_effect_selector_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_effect_selector_context_t *context = effect_context(obj);
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

void light_effect_selector_set_change_cb(lv_obj_t *obj,
                                         light_effect_change_cb_t callback,
                                         void *user_data)
{
    light_effect_selector_context_t *context = effect_context(obj);
    if (!context) {
        return;
    }
    context->callback = callback;
    context->user_data = user_data;
}
