#include "climate_target_toggle.h"

#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"

typedef struct {
    lv_obj_t *buttons[2];
    bool humidity_available;
    climate_target_toggle_cb_t callback;
    void *user_data;
} climate_target_toggle_t;

static void target_clicked_cb(lv_event_t *event)
{
    climate_target_toggle_t *context = lv_event_get_user_data(event);
    if (!context) return;
    bool humidity = lv_event_get_target_obj(event) == context->buttons[1];
    if (humidity && !context->humidity_available) return;
    climate_target_toggle_set_selected(lv_obj_get_parent(context->buttons[0]), humidity);
    if (context->callback) context->callback(humidity, context->user_data);
}

static void target_delete_cb(lv_event_t *event)
{
    lv_free(lv_obj_get_user_data(lv_event_get_target_obj(event)));
}

lv_obj_t *climate_target_toggle_create(lv_obj_t *parent)
{
    if (!parent) return NULL;
    climate_target_toggle_t *context = lv_calloc(1, sizeof(*context));
    if (!context) return NULL;
    lv_obj_t *root = lv_obj_create(parent);
    if (!root) { lv_free(context); return NULL; }
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 96, 48);
    lv_obj_set_style_radius(root, 24, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_user_data(root, context);
    lv_obj_add_event_cb(root, target_delete_cb, LV_EVENT_DELETE, NULL);
    context->humidity_available = true;
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *button = lv_button_create(root);
        context->buttons[i] = button;
        lv_obj_remove_style_all(button);
        lv_obj_set_size(button, 40, 40);
        lv_obj_set_pos(button, i ? 52 : 4, 4);
        lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_event_cb(button, target_clicked_cb, LV_EVENT_CLICKED, context);
        lv_obj_t *icon = lv_label_create(button);
        lv_label_set_text(icon, i ? MDI_SYMBOL_WATER_PERCENT : MDI_SYMBOL_THERMOMETER);
        lv_obj_set_style_text_font(icon, &ha_font_mdi_20, 0);
        lv_obj_set_style_text_color(icon, lv_color_white(), 0);
        lv_obj_center(icon);
    }
    climate_target_toggle_set_selected(root, false);
    return root;
}

void climate_target_toggle_set_selected(lv_obj_t *obj, bool humidity)
{
    climate_target_toggle_t *context = obj ? lv_obj_get_user_data(obj) : NULL;
    if (!context) return;
    for (int i = 0; i < 2; ++i) {
        lv_obj_set_style_bg_color(context->buttons[i], lv_color_hex(0xEEEEEE), 0);
        lv_obj_set_style_bg_opa(context->buttons[i], (i == (int)humidity) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_t *icon = lv_obj_get_child(context->buttons[i], 0);
        lv_obj_set_style_text_color(icon, i == (int)humidity ? lv_color_black() : lv_color_white(), 0);
    }
}

void climate_target_toggle_set_humidity_available(lv_obj_t *obj, bool available)
{
    climate_target_toggle_t *context = obj ? lv_obj_get_user_data(obj) : NULL;
    if (!context) return;
    context->humidity_available = available;
    lv_obj_set_style_opa(context->buttons[1], available ? LV_OPA_COVER : LV_OPA_50, 0);
}

void climate_target_toggle_set_callback(lv_obj_t *obj, climate_target_toggle_cb_t callback, void *user_data)
{
    climate_target_toggle_t *context = obj ? lv_obj_get_user_data(obj) : NULL;
    if (context) { context->callback = callback; context->user_data = user_data; }
}
