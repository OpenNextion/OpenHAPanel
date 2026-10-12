#include "light_power_toggle.h"

#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"

#define POWER_TOGGLE_WIDTH         88
#define POWER_TOGGLE_HEIGHT        230
#define POWER_TOGGLE_RADIUS        20
#define POWER_TOGGLE_KNOB_INSET    6
#define POWER_TOGGLE_KNOB_HEIGHT   110
#define POWER_TOGGLE_KNOB_RADIUS   20
#define POWER_TOGGLE_ANIMATION_MS  180

#define POWER_TOGGLE_ON_TRACK      0x3A2C00
#define POWER_TOGGLE_ON_KNOB       0xFFC107
#define POWER_TOGGLE_OFF_TRACK     0x202020
#define POWER_TOGGLE_OFF_KNOB      0x949494
#define POWER_TOGGLE_PRESSED_COLOR 0xFFFFFF

typedef struct {
    lv_obj_t *root;
    int32_t visual;
    bool on;
    bool disabled;
    light_power_toggle_change_cb_t callback;
    void *user_data;
} light_power_toggle_context_t;

static light_power_toggle_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static int32_t knob_y(const lv_area_t *area, int32_t visual)
{
    const int32_t travel = lv_area_get_height(area) -
                           POWER_TOGGLE_KNOB_INSET * 2 -
                           POWER_TOGGLE_KNOB_HEIGHT;
    return area->y1 + POWER_TOGGLE_KNOB_INSET +
           (100 - LV_CLAMP(0, visual, 100)) * travel / 100;
}

static void draw_rect(lv_layer_t *layer,
                      const lv_area_t *area,
                      uint32_t color,
                      int32_t radius,
                      lv_opa_t opacity)
{
    if (!layer || !area || area->x2 < area->x1 || area->y2 < area->y1) {
        return;
    }
    const lv_area_t *clip = &layer->_clip_area;
    if (area->x1 > clip->x2 || area->x2 < clip->x1 ||
        area->y1 > clip->y2 || area->y2 < clip->y1) {
        return;
    }

    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = lv_color_hex(color);
    descriptor.bg_opa = opacity;
    descriptor.radius = radius;
    lv_draw_rect(layer, &descriptor, area);
}

static void draw_icon(lv_layer_t *layer,
                      const lv_area_t *knob,
                      const char *symbol,
                      lv_opa_t opacity)
{
    if (!layer || !knob || !symbol) {
        return;
    }

    const int32_t line_height = lv_font_get_line_height(&ha_font_mdi_20);
    const int32_t y = knob->y1 +
                      (lv_area_get_height(knob) - line_height) / 2;
    lv_area_t area = {
        .x1 = knob->x1,
        .y1 = y,
        .x2 = knob->x2,
        .y2 = y + line_height - 1,
    };
    lv_draw_label_dsc_t descriptor;
    lv_draw_label_dsc_init(&descriptor);
    descriptor.text = symbol;
    descriptor.font = &ha_font_mdi_20;
    descriptor.color = lv_color_white();
    descriptor.opa = opacity;
    descriptor.align = LV_TEXT_ALIGN_CENTER;
    descriptor.text_static = 1;
    lv_draw_label(layer, &descriptor, &area);
}

static void finish_draw_batch(lv_layer_t *layer)
{
    while (layer && layer->draw_task_head) {
        lv_draw_dispatch_wait_for_request();
        lv_draw_dispatch();
    }
}

static void draw_toggle(lv_event_t *event,
                        light_power_toggle_context_t *context)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    if (!layer || !context || !context->root) {
        return;
    }

    lv_area_t track;
    lv_obj_get_coords(context->root, &track);
    const lv_opa_t opacity = context->disabled ? LV_OPA_40 : LV_OPA_COVER;
    draw_rect(layer,
              &track,
              context->on ? POWER_TOGGLE_ON_TRACK
                          : POWER_TOGGLE_OFF_TRACK,
              POWER_TOGGLE_RADIUS,
              opacity);

    const int32_t y = knob_y(&track, context->visual);
    const lv_area_t knob = {
        .x1 = track.x1 + POWER_TOGGLE_KNOB_INSET,
        .y1 = y,
        .x2 = track.x2 - POWER_TOGGLE_KNOB_INSET,
        .y2 = y + POWER_TOGGLE_KNOB_HEIGHT - 1,
    };
    draw_rect(layer,
              &knob,
              context->on ? POWER_TOGGLE_ON_KNOB
                          : POWER_TOGGLE_OFF_KNOB,
              POWER_TOGGLE_KNOB_RADIUS,
              opacity);
    draw_icon(layer,
              &knob,
              context->on ? MDI_SYMBOL_LIGHTBULB_ON : MDI_SYMBOL_LIGHTBULB_OFF,
              opacity);

    if (!context->disabled &&
        lv_obj_has_state(context->root, LV_STATE_PRESSED)) {
        draw_rect(layer,
                  &track,
                  POWER_TOGGLE_PRESSED_COLOR,
                  POWER_TOGGLE_RADIUS,
                  LV_OPA_10);
    }
    finish_draw_batch(layer);
}

static void visual_animation_cb(void *var, int32_t value)
{
    light_power_toggle_context_t *context = var;
    if (!context || !context->root) {
        return;
    }
    context->visual = value;
    lv_obj_invalidate(context->root);
}

static void animate_to(light_power_toggle_context_t *context, int32_t target)
{
    lv_anim_delete(context, visual_animation_cb);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, context);
    lv_anim_set_exec_cb(&animation, visual_animation_cb);
    lv_anim_set_values(&animation, context->visual, target);
    lv_anim_set_duration(&animation, POWER_TOGGLE_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}

static void toggle_event_cb(lv_event_t *event)
{
    light_power_toggle_context_t *context = lv_event_get_user_data(event);
    if (!context) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        lv_anim_delete(context, visual_animation_cb);
        lv_free(context);
        return;
    }
    if (code == LV_EVENT_DRAW_MAIN) {
        draw_toggle(event, context);
        return;
    }
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (context->disabled) {
        lv_obj_set_state(context->root, LV_STATE_CHECKED, context->on);
        lv_obj_invalidate(context->root);
        return;
    }

    context->on = lv_obj_has_state(context->root, LV_STATE_CHECKED);
    animate_to(context, context->on ? 100 : 0);
    if (context->callback) {
        context->callback(context->on, context->user_data);
    }
}

lv_obj_t *light_power_toggle_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_power_toggle_context_t *context = lv_calloc(1, sizeof(*context));
    if (!context) {
        return NULL;
    }

    lv_obj_t *root = lv_switch_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    context->root = root;
    lv_obj_set_user_data(root, context);
    lv_obj_add_event_cb(root, toggle_event_cb, LV_EVENT_ALL, context);

    /* Keep native switch input/state handling, but draw every pixel here so
     * checked-state styles from the active LVGL theme cannot leak through. */
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(root, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(root, POWER_TOGGLE_WIDTH, POWER_TOGGLE_HEIGHT);
    lv_switch_set_orientation(root, LV_SWITCH_ORIENTATION_VERTICAL);
    lv_obj_set_style_anim_duration(root, 0, 0);

    lv_obj_invalidate(root);
    return root;
}

void light_power_toggle_set_value(lv_obj_t *obj,
                                  bool on,
                                  lv_anim_enable_t animate)
{
    light_power_toggle_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->on = on;
    lv_obj_set_state(obj, LV_STATE_CHECKED, on);
    if (animate == LV_ANIM_ON && context->visual != (on ? 100 : 0)) {
        animate_to(context, on ? 100 : 0);
    }
    else {
        lv_anim_delete(context, visual_animation_cb);
        context->visual = on ? 100 : 0;
        lv_obj_invalidate(obj);
    }
}

void light_power_toggle_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_power_toggle_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->disabled = disabled;
    lv_obj_set_state(obj, LV_STATE_DISABLED, disabled);
    lv_obj_invalidate(obj);
}

void light_power_toggle_set_change_cb(
    lv_obj_t *obj,
    light_power_toggle_change_cb_t callback,
    void *user_data)
{
    light_power_toggle_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->callback = callback;
    context->user_data = user_data;
}
