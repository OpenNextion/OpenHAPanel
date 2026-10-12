#include "light_assumed_power_buttons.h"

#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"

#define POWER_BUTTONS_WIDTH        76
#define POWER_BUTTONS_HEIGHT       230
#define POWER_BUTTON_HEIGHT        110
#define POWER_BUTTON_GAP           10
#define POWER_BUTTON_RADIUS        18

#define POWER_BUTTON_INACTIVE      0x202020
#define POWER_BUTTON_ON_ACTIVE     0xFFC107
#define POWER_BUTTON_OFF_ACTIVE    0x949494

typedef struct {
    lv_obj_t *root;
    lv_obj_t *on_button;
    lv_obj_t *off_button;
    bool known;
    bool on;
    bool disabled;
    light_assumed_power_buttons_change_cb_t callback;
    void *user_data;
} light_assumed_power_buttons_context_t;

static light_assumed_power_buttons_context_t *get_context(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static void refresh_visual(light_assumed_power_buttons_context_t *context)
{
    if (!context || !context->on_button || !context->off_button) {
        return;
    }

    const uint32_t on_color = context->known && context->on
                                  ? POWER_BUTTON_ON_ACTIVE
                                  : POWER_BUTTON_INACTIVE;
    const uint32_t off_color = context->known && !context->on
                                   ? POWER_BUTTON_OFF_ACTIVE
                                   : POWER_BUTTON_INACTIVE;
    lv_obj_set_style_bg_color(context->on_button,
                              lv_color_hex(on_color),
                              0);
    lv_obj_set_style_bg_color(context->off_button,
                              lv_color_hex(off_color),
                              0);
    lv_obj_set_style_opa(context->root,
                         context->disabled ? LV_OPA_40 : LV_OPA_COVER,
                         0);
}

static void button_clicked_cb(lv_event_t *event)
{
    light_assumed_power_buttons_context_t *context =
        lv_event_get_user_data(event);
    if (!context || context->disabled) {
        return;
    }

    context->known = true;
    context->on = lv_event_get_target_obj(event) == context->on_button;
    refresh_visual(context);
    if (context->callback) {
        context->callback(context->on, context->user_data);
    }
}

static void root_delete_cb(lv_event_t *event)
{
    lv_free(lv_event_get_user_data(event));
}

static lv_obj_t *create_button(lv_obj_t *parent,
                               light_assumed_power_buttons_context_t *context,
                               const char *symbol)
{
    lv_obj_t *button = lv_obj_create(parent);
    if (!button) {
        return NULL;
    }
    lv_obj_remove_style_all(button);
    lv_obj_remove_flag(button,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(button, POWER_BUTTONS_WIDTH, POWER_BUTTON_HEIGHT);
    lv_obj_set_style_radius(button, POWER_BUTTON_RADIUS, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(button, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button,
                        button_clicked_cb,
                        LV_EVENT_CLICKED,
                        context);

    lv_obj_t *icon = lv_label_create(button);
    if (!icon) {
        lv_obj_delete(button);
        return NULL;
    }
    lv_label_set_text_static(icon, symbol);
    lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_obj_center(icon);
    return button;
}

lv_obj_t *light_assumed_power_buttons_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    light_assumed_power_buttons_context_t *context =
        lv_calloc(1, sizeof(*context));
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
    lv_obj_add_event_cb(root, root_delete_cb, LV_EVENT_DELETE, context);
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root,
                       LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
                           LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_size(root, POWER_BUTTONS_WIDTH, POWER_BUTTONS_HEIGHT);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_pad_gap(root, POWER_BUTTON_GAP, 0);

    context->on_button = create_button(root,
                                       context,
                                       MDI_SYMBOL_LIGHTBULB_ON);
    context->off_button = create_button(root,
                                        context,
                                        MDI_SYMBOL_LIGHTBULB_OFF);
    if (!context->on_button || !context->off_button) {
        lv_obj_delete(root);
        return NULL;
    }

    refresh_visual(context);
    return root;
}

void light_assumed_power_buttons_set_state(lv_obj_t *obj,
                                           bool known,
                                           bool on)
{
    light_assumed_power_buttons_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->known = known;
    context->on = on;
    refresh_visual(context);
}

void light_assumed_power_buttons_set_disabled(lv_obj_t *obj, bool disabled)
{
    light_assumed_power_buttons_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->disabled = disabled;
    lv_obj_set_state(context->on_button, LV_STATE_DISABLED, disabled);
    lv_obj_set_state(context->off_button, LV_STATE_DISABLED, disabled);
    refresh_visual(context);
}

void light_assumed_power_buttons_set_change_cb(
    lv_obj_t *obj,
    light_assumed_power_buttons_change_cb_t callback,
    void *user_data)
{
    light_assumed_power_buttons_context_t *context = get_context(obj);
    if (!context) {
        return;
    }
    context->callback = callback;
    context->user_data = user_data;
}
