#include "light_preset_colors.h"
#include "light_detail_theme.h"
#include "screen_theme.h"

#define LIGHT_PRESET_COLOR_COUNT  8
#define LIGHT_PRESET_COLUMNS      4
#define LIGHT_PRESET_BUTTON_SIZE  56
#define LIGHT_PRESET_GAP          16
#define LIGHT_PRESET_PARENT_WIDTH 320
#define LIGHT_PRESET_PARENT_HEIGHT 160
#define LIGHT_PRESET_BUTTON_DRAW_SIZE (LIGHT_PRESET_BUTTON_SIZE)
#define LIGHT_PRESET_BUTTON_PRESSED_SIZE \
    (LIGHT_PRESET_BUTTON_DRAW_SIZE + 4)
#define LIGHT_PRESET_TILE_HEIGHT 56
#define LIGHT_PRESET_TILE_BUTTON_HEIGHT 36
#define LIGHT_PRESET_TILE_GAP 8
#define LIGHT_PRESET_OFFSET_X \
    ((LIGHT_PRESET_PARENT_WIDTH - \
      (LIGHT_PRESET_COLUMNS * LIGHT_PRESET_BUTTON_SIZE + \
       (LIGHT_PRESET_COLUMNS - 1) * LIGHT_PRESET_GAP)) / \
     2)
#define LIGHT_PRESET_OFFSET_Y \
    ((LIGHT_PRESET_PARENT_HEIGHT - \
      (2 * LIGHT_PRESET_BUTTON_SIZE + LIGHT_PRESET_GAP)) / \
     2)

typedef struct {
    lv_obj_t *buttons[LIGHT_PRESET_COLOR_COUNT];
    light_favorite_color_t colors[LIGHT_PRESET_COLOR_COUNT];
    size_t count;
    size_t tile_limit;
    bool tile_layout;
    bool disabled;
    lv_obj_t *root;
    light_preset_select_cb_t callback;
    void *user_data;
} light_preset_colors_context_t;

static light_preset_colors_context_t *light_preset_colors_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static void light_preset_colors_set_button_geometry(lv_obj_t *button,
                                                    size_t index,
                                                    bool pressed,
                                                    const light_preset_colors_context_t *context)
{
    if (context->tile_layout) {
        const size_t visible = LV_MIN(context->count, context->tile_limit);
        if (visible == 0) {
            return;
        }
        const int32_t width = lv_obj_get_content_width(context->root);
        const int32_t usable = LV_MAX(visible,
            width - (int32_t)(visible - 1) * LIGHT_PRESET_TILE_GAP);
        const int32_t x = (int32_t)index * LIGHT_PRESET_TILE_GAP +
                          (int32_t)index * usable / (int32_t)visible;
        const int32_t next = (int32_t)(index + 1) * usable /
                             (int32_t)visible;
        const int32_t height = LIGHT_PRESET_TILE_BUTTON_HEIGHT +
                               (pressed ? 4 : 0);
        lv_obj_set_pos(button, x,
                       (LIGHT_PRESET_TILE_HEIGHT - height) / 2);
        lv_obj_set_size(button, next - (int32_t)index * usable /
                                  (int32_t)visible, height);
        return;
    }
    const int32_t size = pressed ? LIGHT_PRESET_BUTTON_PRESSED_SIZE
                                 : LIGHT_PRESET_BUTTON_DRAW_SIZE;
    const int32_t grow_offset =
        (size - LIGHT_PRESET_BUTTON_DRAW_SIZE) / 2;
    const int32_t x =
        LIGHT_PRESET_OFFSET_X +
        (LIGHT_PRESET_BUTTON_SIZE - LIGHT_PRESET_BUTTON_DRAW_SIZE) / 2 +
        (int32_t)(index % LIGHT_PRESET_COLUMNS) *
            (LIGHT_PRESET_BUTTON_SIZE + LIGHT_PRESET_GAP) -
        grow_offset;
    const int32_t y =
        LIGHT_PRESET_OFFSET_Y +
        (LIGHT_PRESET_BUTTON_SIZE - LIGHT_PRESET_BUTTON_DRAW_SIZE) / 2 +
        (int32_t)(index / LIGHT_PRESET_COLUMNS) *
            (LIGHT_PRESET_BUTTON_SIZE + LIGHT_PRESET_GAP) -
        grow_offset;

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, size, size);
}

static bool light_preset_color_needs_border(light_rgb_t color)
{
    const uint8_t min_ch = LV_MIN(color.red,
                                   LV_MIN(color.green, color.blue));
    const uint8_t max_ch = LV_MAX(color.red,
                                   LV_MAX(color.green, color.blue));
    return min_ch >= 200 && (max_ch - min_ch) <= 60;
}

static void light_preset_colors_refresh(light_preset_colors_context_t *context)
{
    if (!context) {
        return;
    }
    const bool light_theme = light_detail_is_light_theme();
    for (size_t i = 0; i < LIGHT_PRESET_COLOR_COUNT; i++) {
        lv_obj_t *button = context->buttons[i];
        if (i < LV_MIN(context->count,
                       context->tile_layout ? context->tile_limit
                                            : LIGHT_PRESET_COLOR_COUNT)) {
            lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
            light_preset_colors_set_button_geometry(button, i, false, context);
            const light_rgb_t original = context->colors[i].display_rgb;
            const uint8_t gray = (uint8_t)(
                ((uint32_t)original.red * 77 +
                 (uint32_t)original.green * 150 +
                 (uint32_t)original.blue * 29 + 128) / 256);
            const light_rgb_t display = context->disabled
                ? (light_rgb_t){gray, gray, gray} : original;
            lv_obj_set_style_bg_color(
                button,
                lv_color_make(display.red, display.green, display.blue),
                0);
            lv_obj_set_style_bg_color(button,
                lv_color_make(gray, gray, gray), LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_opa(button, LV_OPA_50, LV_STATE_DISABLED);
            lv_obj_set_style_recolor_opa(button, LV_OPA_TRANSP,
                                          LV_STATE_DISABLED);
            if (light_theme &&
                light_preset_color_needs_border(
                    context->colors[i].display_rgb)) {
                const light_rgb_t c = display;
                const lv_color_t border_color =
                    lv_color_make((uint8_t)((uint16_t)c.red * 224 / 255),
                                  (uint8_t)((uint16_t)c.green * 224 / 255),
                                  (uint8_t)((uint16_t)c.blue * 224 / 255));
                lv_obj_set_style_border_width(button, 2, 0);
                lv_obj_set_style_border_color(button, border_color, 0);
                lv_obj_set_style_border_opa(button, LV_OPA_COVER, 0);
                lv_obj_set_style_border_color(button, border_color,
                                              LV_STATE_DISABLED);
                lv_obj_set_style_border_opa(button, LV_OPA_50,
                                            LV_STATE_DISABLED);
                lv_obj_set_style_border_width(button, 2,
                                              LV_STATE_DISABLED);
                lv_obj_set_style_border_width(button, 2,
                                              LV_STATE_PRESSED);
                lv_obj_set_style_border_color(button, border_color,
                                              LV_STATE_PRESSED);
                lv_obj_set_style_border_opa(button, LV_OPA_COVER,
                                            LV_STATE_PRESSED);
            }
            else {
                lv_obj_set_style_border_width(button, 0, 0);
                lv_obj_set_style_border_width(button, 0,
                                              LV_STATE_DISABLED);
                lv_obj_set_style_border_width(button, 0,
                                              LV_STATE_PRESSED);
            }
        }
        else {
            lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_border_width(button, 0, 0);
        }
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
    if (!context) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    for (size_t i = 0; i < LIGHT_PRESET_COLOR_COUNT; i++) {
        if (target == context->buttons[i]) {
            if (code == LV_EVENT_PRESSED && !context->disabled) {
                light_preset_colors_set_button_geometry(target, i, true,
                                                         context);
            }
            else if (code == LV_EVENT_RELEASED ||
                     code == LV_EVENT_PRESS_LOST) {
                light_preset_colors_set_button_geometry(target, i, false,
                                                         context);
            }
            else if (code == LV_EVENT_CLICKED && !context->disabled &&
                     i < context->count && context->callback) {
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

static void light_preset_colors_size_cb(lv_event_t *event)
{
    light_preset_colors_context_t *context = lv_event_get_user_data(event);
    if (context && context->tile_layout) {
        light_preset_colors_refresh(context);
    }
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

    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    lv_obj_set_user_data(root, context);
    context->root = root;
    lv_obj_add_event_cb(root,
                        light_preset_colors_delete_cb,
                        LV_EVENT_DELETE,
                        context);
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root, light_preset_colors_size_cb,
                        LV_EVENT_SIZE_CHANGED, context);
    /* Keep a fixed drawing canvas around the two rows.  Buttons use absolute
     * positions so their pressed transform is not constrained by a grid
     * cell that remains at the untransformed size. */
    lv_obj_set_size(root,
                    LIGHT_PRESET_PARENT_WIDTH,
                    LIGHT_PRESET_PARENT_HEIGHT);

    for (size_t i = 0; i < LIGHT_PRESET_COLOR_COUNT; i++) {
        context->buttons[i] = lv_button_create(root);
        light_preset_colors_set_button_geometry(context->buttons[i], i, false,
                                                 context);
        lv_obj_set_style_radius(context->buttons[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(context->buttons[i], 0, 0);
        lv_obj_set_style_border_width(context->buttons[i], 0, LV_STATE_PRESSED);
        /* The default button theme enlarges only the draw area.  Keep that
         * transform disabled because this component changes the object's
         * real coordinates and size while it is pressed. */
        lv_obj_set_style_transform_width(context->buttons[i],
                                         0,
                                         LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(context->buttons[i],
                                          0,
                                          LV_STATE_PRESSED);
        lv_obj_add_event_cb(context->buttons[i],
                            light_preset_colors_event_cb,
                            LV_EVENT_PRESSED,
                            context);
        lv_obj_add_event_cb(context->buttons[i],
                            light_preset_colors_event_cb,
                            LV_EVENT_RELEASED,
                            context);
        lv_obj_add_event_cb(context->buttons[i],
                            light_preset_colors_event_cb,
                            LV_EVENT_PRESS_LOST,
                            context);
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
    light_preset_colors_refresh(context);
}

void light_preset_colors_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_preset_colors_context_t *context = light_preset_colors_context(obj);
    if (!context) {
        return;
    }
    context->disabled = disabled;
    light_preset_colors_refresh(context);
}

void light_preset_colors_set_tile_layout(lv_obj_t *obj, size_t limit)
{
    light_preset_colors_context_t *context = light_preset_colors_context(obj);
    if (!context) {
        return;
    }
    const size_t visible_limit = LV_MIN(limit, LIGHT_PRESET_COLOR_COUNT);
    if (context->tile_layout && context->tile_limit == visible_limit) {
        return;
    }
    context->tile_layout = true;
    context->tile_limit = visible_limit;
    lv_obj_set_size(obj, LV_PCT(100), LIGHT_PRESET_TILE_HEIGHT);
    for (size_t i = 0; i < LIGHT_PRESET_COLOR_COUNT; i++) {
        lv_obj_set_style_radius(context->buttons[i], 12, 0);
    }
    light_preset_colors_refresh(context);
}

void light_preset_colors_refresh_theme(lv_obj_t *obj)
{
    light_preset_colors_refresh(light_preset_colors_context(obj));
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
