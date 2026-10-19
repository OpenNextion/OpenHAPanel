#include "light_preset_colors.h"

#define LIGHT_PRESET_COLOR_COUNT 8
#define LIGHT_PRESET_COLUMNS     4
#define LIGHT_PRESET_BUTTON_SIZE 56
#define LIGHT_PRESET_GAP         16
/* Leave room for the button theme's pressed-state scale animation. */
#define LIGHT_PRESET_PRESS_OVERFLOW 8

typedef struct {
    lv_obj_t *buttons[LIGHT_PRESET_COLOR_COUNT];
    light_favorite_color_t colors[LIGHT_PRESET_COLOR_COUNT];
    size_t count;
    int selected_index;
    bool disabled;
    light_preset_select_cb_t callback;
    void *user_data;
} light_preset_colors_context_t;

static light_preset_colors_context_t *light_preset_colors_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static void light_preset_colors_refresh(light_preset_colors_context_t *context)
{
    if (!context) {
        return;
    }
    for (size_t i = 0; i < LIGHT_PRESET_COLOR_COUNT; i++) {
        lv_obj_t *button = context->buttons[i];
        if (i < context->count) {
            lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(
                button,
                lv_color_make(context->colors[i].display_rgb.red,
                              context->colors[i].display_rgb.green,
                              context->colors[i].display_rgb.blue),
                0);
        }
        else {
            lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_style_border_width(button, 0, 0);
        if (context->disabled) {
            lv_obj_add_state(button, LV_STATE_DISABLED);
        }
        else {
            lv_obj_remove_state(button, LV_STATE_DISABLED);
        }
    }
}

static void light_preset_colors_event_cb(lv_event_t *event)
{
    light_preset_colors_context_t *context = lv_event_get_user_data(event);
    if (!context || context->disabled) {
        return;
    }
    lv_obj_t *target = lv_event_get_target_obj(event);
    for (size_t i = 0; i < context->count; i++) {
        if (target == context->buttons[i]) {
            context->selected_index = (int)i;
            light_preset_colors_refresh(context);
            if (context->callback) {
                context->callback(i, &context->colors[i], context->user_data);
            }
            return;
        }
    }
}

static void light_preset_colors_delete_cb(lv_event_t *event)
{
    lv_free(lv_event_get_user_data(event));
}

lv_obj_t *light_preset_colors_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_preset_colors_context_t *context = lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }
    context->count = 0;
    context->selected_index = -1;

    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    lv_obj_set_user_data(root, context);
    lv_obj_add_event_cb(root,
                        light_preset_colors_delete_cb,
                        LV_EVENT_DELETE,
                        context);
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root,
                    LIGHT_PRESET_COLUMNS * LIGHT_PRESET_BUTTON_SIZE +
                        (LIGHT_PRESET_COLUMNS - 1) * LIGHT_PRESET_GAP +
                        2 * LIGHT_PRESET_PRESS_OVERFLOW,
                    2 * LIGHT_PRESET_BUTTON_SIZE + LIGHT_PRESET_GAP +
                        2 * LIGHT_PRESET_PRESS_OVERFLOW);
    static const int32_t column_dsc[] = {
        LIGHT_PRESET_BUTTON_SIZE,
        LIGHT_PRESET_BUTTON_SIZE,
        LIGHT_PRESET_BUTTON_SIZE,
        LIGHT_PRESET_BUTTON_SIZE,
        LV_GRID_TEMPLATE_LAST,
    };
    static const int32_t row_dsc[] = {
        LIGHT_PRESET_BUTTON_SIZE,
        LIGHT_PRESET_BUTTON_SIZE,
        LV_GRID_TEMPLATE_LAST,
    };
    lv_obj_set_grid_dsc_array(root, column_dsc, row_dsc);
    lv_obj_set_style_pad_all(root, LIGHT_PRESET_PRESS_OVERFLOW, 0);
    lv_obj_set_style_pad_row(root, LIGHT_PRESET_GAP, 0);
    lv_obj_set_style_pad_column(root, LIGHT_PRESET_GAP, 0);

    for (size_t i = 0; i < LIGHT_PRESET_COLOR_COUNT; i++) {
        context->buttons[i] = lv_button_create(root);
        lv_obj_set_size(context->buttons[i],
                        LIGHT_PRESET_BUTTON_SIZE,
                        LIGHT_PRESET_BUTTON_SIZE);
        lv_obj_set_grid_cell(context->buttons[i],
                             LV_GRID_ALIGN_CENTER,
                             i % LIGHT_PRESET_COLUMNS,
                             1,
                             LV_GRID_ALIGN_CENTER,
                             i / LIGHT_PRESET_COLUMNS,
                             1);
        lv_obj_set_style_radius(context->buttons[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(context->buttons[i], 0, 0);
        lv_obj_set_style_border_width(context->buttons[i], 0, LV_STATE_PRESSED);
        lv_obj_add_event_cb(context->buttons[i],
                            light_preset_colors_event_cb,
                            LV_EVENT_CLICKED,
                            context);
    }
    light_preset_colors_refresh(context);
    return root;
}

void light_preset_colors_set_palette(lv_obj_t *obj,
                                     const light_favorite_color_t *colors,
                                     size_t count)
{
    light_preset_colors_context_t *context = light_preset_colors_context(obj);
    if (!context || (!colors && count > 0)) {
        return;
    }
    context->count = LV_MIN(count, LIGHT_PRESET_COLOR_COUNT);
    for (size_t i = 0; i < context->count; i++) {
        context->colors[i] = colors[i];
    }
    if (context->selected_index >= (int)context->count) {
        context->selected_index = -1;
    }
    const size_t rows = (context->count + LIGHT_PRESET_COLUMNS - 1) /
                        LIGHT_PRESET_COLUMNS;
    lv_obj_set_height(obj,
                      rows > 0
                          ? (int32_t)(rows * LIGHT_PRESET_BUTTON_SIZE +
                                      (rows - 1) * LIGHT_PRESET_GAP +
                                      2 * LIGHT_PRESET_PRESS_OVERFLOW)
                          : 0);
    light_preset_colors_refresh(context);
}

void light_preset_colors_set_selected(lv_obj_t *obj, int selected_index)
{
    light_preset_colors_context_t *context = light_preset_colors_context(obj);
    if (!context) {
        return;
    }
    context->selected_index = selected_index >= 0 &&
                                      selected_index < (int)context->count
                                  ? selected_index
                                  : -1;
    light_preset_colors_refresh(context);
}

void light_preset_colors_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_preset_colors_context_t *context = light_preset_colors_context(obj);
    if (!context) {
        return;
    }
    context->disabled = disabled;
    lv_obj_set_style_opa(obj, disabled ? LV_OPA_50 : LV_OPA_COVER, 0);
    light_preset_colors_refresh(context);
}

void light_preset_colors_set_select_cb(lv_obj_t *obj,
                                       light_preset_select_cb_t callback,
                                       void *user_data)
{
    light_preset_colors_context_t *context = light_preset_colors_context(obj);
    if (!context) {
        return;
    }
    context->callback = callback;
    context->user_data = user_data;
}
