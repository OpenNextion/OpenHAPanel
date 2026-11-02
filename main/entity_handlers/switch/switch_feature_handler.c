#include "switch_feature_handler.h"

#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "common/ha_control_toggle/ha_control_toggle.h"
#include "esp_log.h"

static const char *TAG = "switch_feature_handler";

#define SWITCH_GLYPH_SIZE   14
#define SWITCH_GLYPH_STROKE 2

static bool switch_feature_supports(ha_entity_store_t *store,
                                    const char *entity_id,
                                    screen_feature_type_t type)
{
    (void)store;
    (void)entity_id;
    return type == SCREEN_FEATURE_SWITCH_TOGGLE;
}

static void toggle_changed(bool on, void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id) {
        return;
    }
    ha_entity_store_t *store = app_get_ha_entity_store();
    ha_entity_snapshot_t snapshot = {0};
    if (!store ||
        ha_entity_store_get(store, instance->entity_id, &snapshot) != ESP_OK ||
        entity_handler_snapshot_is_unavailable(&snapshot)) {
        return;
    }
    const char *service = on ? "turn_on" : "turn_off";
    esp_err_t err = app_ha_call_entity_service("switch", service,
                                                instance->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "switch.%s for %s failed: %s",
                 service, instance->entity_id, esp_err_to_name(err));
    }
}

static void build_ring(lv_obj_t *parent)
{
    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_remove_style_all(ring);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ring, SWITCH_GLYPH_SIZE, SWITCH_GLYPH_SIZE);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, SWITCH_GLYPH_STROKE, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, 0);
}

static void build_power_glyph(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(box, SWITCH_GLYPH_SIZE, SWITCH_GLYPH_SIZE);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_center(box);

    build_ring(box);

    lv_obj_t *mask = lv_obj_create(box);
    lv_obj_remove_style_all(mask);
    lv_obj_remove_flag(mask, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(mask, SWITCH_GLYPH_SIZE / 2 + 1, SWITCH_GLYPH_STROKE + 2);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0xF9C000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_COVER, 0);
    lv_obj_align(mask, LV_ALIGN_TOP_MID, 0, -1);

    lv_obj_t *stem = lv_obj_create(box);
    lv_obj_remove_style_all(stem);
    lv_obj_remove_flag(stem, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(stem, SWITCH_GLYPH_STROKE, 8);
    lv_obj_set_style_radius(stem, SWITCH_GLYPH_STROKE / 2, 0);
    lv_obj_set_style_bg_color(stem, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, 0);
    lv_obj_align(stem, LV_ALIGN_TOP_MID, 0, -2);
}

static void rebuild_icon(lv_obj_t *toggle, bool on)
{
    lv_obj_t *knob = ha_control_toggle_get_knob(toggle);
    if (!knob) {
        return;
    }
    lv_obj_clean(knob);
    if (on) {
        build_power_glyph(knob);
    } else {
        build_ring(knob);
    }
}

static esp_err_t switch_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config ||
        instance->config->type != SCREEN_FEATURE_SWITCH_TOGGLE) {
        return ESP_ERR_INVALID_ARG;
    }
    instance->control = ha_control_toggle_create(instance->host);
    if (!instance->control) {
        return ESP_ERR_NO_MEM;
    }
    ha_control_toggle_set_disabled(instance->control, true);
    ha_control_toggle_set_change_cb(instance->control, toggle_changed,
                                    instance);
    rebuild_icon(instance->control, false);
    return ESP_OK;
}

static void switch_feature_update(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) {
        return;
    }
    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_unavailable(snapshot);
    const bool on = available && strcmp(snapshot->state, "on") == 0;
    ha_control_toggle_set_active_color(instance->control,
                                       lv_color_hex(0xF9C000));
    ha_control_toggle_set_value(instance->control, on, LV_ANIM_ON);
    ha_control_toggle_set_disabled(instance->control, !available);
    rebuild_icon(instance->control, on);
}

const entity_feature_handler_t switch_feature_handler = {
    .supports = switch_feature_supports,
    .create = switch_feature_create,
    .update = switch_feature_update,
};
