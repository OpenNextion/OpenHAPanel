#include "climate_step_buttons.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "climate_outlined_icon_button.h"

typedef struct {
    lv_obj_t *buttons[2];
    climate_step_cb_t callback;
    void *user_data;
} climate_step_buttons_t;

static void clicked(lv_event_t *event)
{
    climate_step_buttons_t *context = lv_event_get_user_data(event);
    if (!context || !context->callback) return;
    context->callback(lv_event_get_target_obj(event) == context->buttons[0] ? -1 : 1,
                      context->user_data);
}

static void deleted(lv_event_t *event)
{
    lv_free(lv_obj_get_user_data(lv_event_get_target_obj(event)));
}

lv_obj_t *climate_step_buttons_create(lv_obj_t *parent)
{
    if (!parent) return NULL;
    climate_step_buttons_t *context = lv_calloc(1, sizeof(*context));
    if (!context) return NULL;
    lv_obj_t *root = lv_obj_create(parent);
    if (!root) { lv_free(context); return NULL; }
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 104, 40);
    lv_obj_set_user_data(root, context);
    lv_obj_add_event_cb(root, deleted, LV_EVENT_DELETE, NULL);
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *button = climate_outlined_icon_button_create(root);
        if (!button) { lv_obj_delete(root); return NULL; }
        context->buttons[i] = button;
        climate_outlined_icon_button_set_size(button, 40);
        climate_outlined_icon_button_set_icon_size(button, 24);
        climate_outlined_icon_button_set_symbol(button, i ? MDI_SYMBOL_PLUS : MDI_SYMBOL_MINUS);
        climate_outlined_icon_button_set_outline_color(button, lv_color_white());
        climate_outlined_icon_button_set_text_color(button, lv_color_white());
        climate_outlined_icon_button_set_text_font(button, &ha_font_mdi_24);
        lv_obj_set_x(button, i ? 64 : 0);
        lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, context);
    }
    return root;
}

void climate_step_buttons_set_callback(lv_obj_t *obj, climate_step_cb_t callback, void *user_data)
{
    climate_step_buttons_t *context = obj ? lv_obj_get_user_data(obj) : NULL;
    if (context) { context->callback = callback; context->user_data = user_data; }
}

void climate_step_buttons_set_disabled(lv_obj_t *obj, bool disabled)
{
    climate_step_buttons_t *context = obj ? lv_obj_get_user_data(obj) : NULL;
    if (!context) return;
    for (int i = 0; i < 2; ++i)
        climate_outlined_icon_button_set_disabled(context->buttons[i], disabled);
}
