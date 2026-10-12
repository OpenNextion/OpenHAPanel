#include "light_mode_bar.h"

#include <stddef.h>

#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"

#define MODE_BAR_BUTTON_SIZE 40
#define MODE_BAR_HEIGHT      48
#define MODE_BAR_PAD         4
#define MODE_BAR_GAP         8
#define MODE_BAR_SEPARATOR_W 1
#define MODE_BAR_SEPARATOR_H 37

#define MODE_BAR_COLOR       0x202020
#define MODE_ICON_COLOR      0xFFFFFF
#define MODE_SELECTED_COLOR  0xF5F5F5
#define MODE_SELECTED_ICON   0x303030
#define MODE_SEPARATOR_COLOR 0x5C5C5C

typedef struct {
    lv_obj_t *root;
    lv_obj_t *separator;
    lv_obj_t *power_button;
    lv_obj_t *power_icon;
    lv_obj_t *mode_buttons[3];
    lv_obj_t *mode_icons[3];
    uint32_t mode_mask;
    light_mode_t active_mode;
    bool power_on;
    bool disabled;
    light_mode_bar_event_cb_t callback;
    void *user_data;
} light_mode_bar_context_t;

static light_mode_bar_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static void delete_cb(lv_event_t *event)
{
    lv_free(lv_event_get_user_data(event));
}

static void button_cb(lv_event_t *event)
{
    light_mode_bar_context_t *context = lv_event_get_user_data(event);
    if (!context || context->disabled || !context->callback) {
        return;
    }

    lv_obj_t *target = lv_event_get_target_obj(event);
    light_mode_bar_event_t value = {0};
    if (target == context->power_button) {
        value.type = LIGHT_MODE_BAR_EVENT_POWER;
        value.mode = context->active_mode;
    }
    else {
        value.type = LIGHT_MODE_BAR_EVENT_MODE;
        bool found = false;
        for (size_t i = 0; i < 3; i++) {
            if (target == context->mode_buttons[i]) {
                value.mode = (light_mode_t)i;
                found = true;
                break;
            }
        }
        if (!found) {
            return;
        }
    }
    context->callback(&value, context->user_data);
}

static lv_obj_t *add_button(lv_obj_t *parent,
                            light_mode_bar_context_t *context)
{
    lv_obj_t *button = lv_button_create(parent);
    if (!button) {
        return NULL;
    }
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, MODE_BAR_BUTTON_SIZE, MODE_BAR_BUTTON_SIZE);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button,
                              lv_color_hex(MODE_SELECTED_COLOR),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button,
                            LV_OPA_20,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, button_cb, LV_EVENT_CLICKED, context);
    return button;
}

static lv_obj_t *add_mdi_icon(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *icon = lv_label_create(parent);
    if (!icon) {
        return NULL;
    }
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &ha_font_mdi_24, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(MODE_ICON_COLOR), 0);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(icon);
    return icon;
}

static lv_obj_t *add_image_icon(lv_obj_t *parent,
                                const lv_image_dsc_t *source)
{
    lv_obj_t *icon = lv_image_create(parent);
    if (!icon) {
        return NULL;
    }
    lv_image_set_src(icon, source);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(icon);
    return icon;
}

static void set_button_disabled(lv_obj_t *button, bool disabled)
{
    if (!button) {
        return;
    }
    if (disabled) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }
    else {
        lv_obj_remove_state(button, LV_STATE_DISABLED);
    }
}

static void refresh(light_mode_bar_context_t *context)
{
    if (!context) {
        return;
    }

    const bool has_alternate =
        (context->mode_mask &
         (LIGHT_MODE_MASK_RGB | LIGHT_MODE_MASK_COLOR_TEMP)) != 0;
    const bool visible[3] = {
        (context->mode_mask & LIGHT_MODE_MASK_BRIGHTNESS) != 0 &&
            has_alternate,
        (context->mode_mask & LIGHT_MODE_MASK_RGB) != 0,
        (context->mode_mask & LIGHT_MODE_MASK_COLOR_TEMP) != 0,
    };
    bool has_visible_mode = false;

    for (size_t i = 0; i < 3; i++) {
        lv_obj_t *button = context->mode_buttons[i];
        if (!button) {
            continue;
        }
        if (visible[i]) {
            lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
            has_visible_mode = true;
        }
        else {
            lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
        }

        const bool selected = visible[i] &&
                              context->active_mode == (light_mode_t)i;
        lv_obj_set_style_bg_opa(
            button,
            i == LIGHT_MODE_BRIGHTNESS && selected
                ? LV_OPA_COVER
                : LV_OPA_TRANSP,
            LV_PART_MAIN);
        lv_obj_set_style_bg_color(button,
                                  lv_color_hex(MODE_SELECTED_COLOR),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_width(
            button,
            i != LIGHT_MODE_BRIGHTNESS && selected ? 2 : 0,
            LV_PART_MAIN);
        lv_obj_set_style_border_color(button,
                                      lv_color_hex(MODE_SELECTED_COLOR),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(button, LV_OPA_COVER, LV_PART_MAIN);
        set_button_disabled(button, context->disabled);
    }

    if (context->mode_icons[LIGHT_MODE_BRIGHTNESS]) {
        lv_obj_set_style_text_color(
            context->mode_icons[LIGHT_MODE_BRIGHTNESS],
            lv_color_hex(context->active_mode == LIGHT_MODE_BRIGHTNESS &&
                                 visible[LIGHT_MODE_BRIGHTNESS]
                             ? MODE_SELECTED_ICON
                             : MODE_ICON_COLOR),
            0);
    }
    if (context->separator) {
        if (has_visible_mode) {
            lv_obj_remove_flag(context->separator, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(context->separator, LV_OBJ_FLAG_HIDDEN);
        }
    }
    set_button_disabled(context->power_button, context->disabled);
    lv_obj_set_style_opa(context->root,
                         context->disabled ? LV_OPA_40 : LV_OPA_COVER,
                         0);
}

lv_obj_t *light_mode_bar_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_mode_bar_context_t *context = lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }

    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    context->root = root;
    lv_obj_set_user_data(root, context);
    lv_obj_add_event_cb(root, delete_cb, LV_EVENT_DELETE, context);
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, LV_SIZE_CONTENT, MODE_BAR_HEIGHT);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(root,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(root, MODE_BAR_PAD, 0);
    lv_obj_set_style_pad_ver(root, MODE_BAR_PAD, 0);
    lv_obj_set_style_pad_gap(root, MODE_BAR_GAP, 0);
    lv_obj_set_style_radius(root, 30, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(MODE_BAR_COLOR), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    context->power_button = add_button(root, context);
    if (context->power_button) {
        context->power_icon = add_mdi_icon(context->power_button,
                                           MDI_SYMBOL_POWER);
    }

    context->separator = lv_obj_create(root);
    if (context->separator) {
        lv_obj_remove_style_all(context->separator);
        lv_obj_set_size(context->separator,
                        MODE_BAR_SEPARATOR_W,
                        MODE_BAR_SEPARATOR_H);
        lv_obj_set_style_bg_color(context->separator,
                                  lv_color_hex(MODE_SEPARATOR_COLOR),
                                  0);
        lv_obj_set_style_bg_opa(context->separator, LV_OPA_COVER, 0);
        lv_obj_remove_flag(context->separator,
                           LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }

    for (size_t i = 0; i < 3; i++) {
        context->mode_buttons[i] = add_button(root, context);
    }
    if (context->mode_buttons[LIGHT_MODE_BRIGHTNESS]) {
        context->mode_icons[LIGHT_MODE_BRIGHTNESS] =
            add_mdi_icon(context->mode_buttons[LIGHT_MODE_BRIGHTNESS],
                         MDI_SYMBOL_BRIGHTNESS_6);
    }
    if (context->mode_buttons[LIGHT_MODE_RGB]) {
        context->mode_icons[LIGHT_MODE_RGB] =
            add_image_icon(context->mode_buttons[LIGHT_MODE_RGB],
                           &rgb_color_30);
    }
    if (context->mode_buttons[LIGHT_MODE_COLOR_TEMP]) {
        context->mode_icons[LIGHT_MODE_COLOR_TEMP] =
            add_image_icon(context->mode_buttons[LIGHT_MODE_COLOR_TEMP],
                           &color_temp_30);
    }

    context->mode_mask = LIGHT_MODE_MASK_BRIGHTNESS;
    context->active_mode = LIGHT_MODE_BRIGHTNESS;
    refresh(context);
    return root;
}

void light_mode_bar_set_modes(lv_obj_t *obj, uint32_t mode_mask)
{
    light_mode_bar_context_t *context = get_context(obj);
    if (context) {
        context->mode_mask = mode_mask;
        refresh(context);
    }
}

void light_mode_bar_set_active_mode(lv_obj_t *obj, light_mode_t mode)
{
    light_mode_bar_context_t *context = get_context(obj);
    if (context) {
        context->active_mode = mode;
        refresh(context);
    }
}

void light_mode_bar_set_power(lv_obj_t *obj, bool on)
{
    light_mode_bar_context_t *context = get_context(obj);
    if (context) {
        context->power_on = on;
    }
}

void light_mode_bar_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_mode_bar_context_t *context = get_context(obj);
    if (context) {
        context->disabled = disabled;
        refresh(context);
    }
}

void light_mode_bar_set_event_cb(lv_obj_t *obj,
                                 light_mode_bar_event_cb_t callback,
                                 void *user_data)
{
    light_mode_bar_context_t *context = get_context(obj);
    if (context) {
        context->callback = callback;
        context->user_data = user_data;
    }
}
