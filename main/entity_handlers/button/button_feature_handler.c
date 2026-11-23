// main/entity_handlers/button/button_feature_handler.c
#include "button_feature_handler.h"

#include <string.h>

#include "app.h"
#include "common/ha_control_button/ha_control_button.h"
#include "esp_log.h"
#include "ha_entity_store.h"
#include "lvgl.h"

static const char *TAG = "button_feature_handler";

/* Maximum presses tracked in parallel while waiting for a state change. */
#define BUTTON_FEATURE_PENDING_MAX 8
/* How long to wait for the entity state to change before reporting error. */
#define BUTTON_FEATURE_TIMEOUT_MS 3000

typedef struct {
    lv_obj_t *control;
    char pending_state[HA_ENTITY_STORE_STATE_MAX];
    lv_timer_t *timeout_timer;
    bool active;
} button_feature_pending_t;

static button_feature_pending_t s_pending[BUTTON_FEATURE_PENDING_MAX];

static button_feature_pending_t *pending_find(lv_obj_t *control)
{
    for (size_t i = 0; i < BUTTON_FEATURE_PENDING_MAX; i++) {
        if (s_pending[i].active && s_pending[i].control == control) {
            return &s_pending[i];
        }
    }
    return NULL;
}

static button_feature_pending_t *pending_acquire(lv_obj_t *control)
{
    button_feature_pending_t *existing = pending_find(control);
    if (existing) {
        return existing;
    }
    for (size_t i = 0; i < BUTTON_FEATURE_PENDING_MAX; i++) {
        if (!s_pending[i].active) {
            memset(&s_pending[i], 0, sizeof(s_pending[i]));
            s_pending[i].control = control;
            s_pending[i].active = true;
            return &s_pending[i];
        }
    }
    return NULL;
}

static void pending_release(button_feature_pending_t *pending)
{
    if (!pending) {
        return;
    }
    if (pending->timeout_timer) {
        lv_timer_del(pending->timeout_timer);
    }
    pending->control = NULL;
    pending->timeout_timer = NULL;
    pending->pending_state[0] = '\0';
    pending->active = false;
}

static void pending_timeout_cb(lv_timer_t *timer)
{
    button_feature_pending_t *pending = lv_timer_get_user_data(timer);
    lv_timer_del(timer);
    if (!pending || !pending->active) {
        return;
    }
    pending->timeout_timer = NULL;
    if (pending->control && lv_obj_is_valid(pending->control)) {
        ha_control_button_finish_loading(pending->control, false);
    }
    pending->control = NULL;
    pending->pending_state[0] = '\0';
    pending->active = false;
}

static bool button_feature_supports(ha_entity_store_t *store,
                                    const char *entity_id,
                                    screen_feature_type_t type)
{
    (void)store;
    (void)entity_id;
    return type == SCREEN_FEATURE_BUTTON;
}

static void button_pressed(void *user_data)
{
    entity_feature_instance_t *instance = user_data;
    if (!instance || !instance->entity_id || !instance->control) {
        return;
    }
    if (ha_control_button_is_busy(instance->control)) {
        return;
    }

    button_feature_pending_t *pending = pending_acquire(instance->control);
    if (pending) {
        pending->pending_state[0] = '\0';
        ha_entity_store_t *store = app_get_ha_entity_store();
        if (store) {
            ha_entity_snapshot_t snapshot = {0};
            if (ha_entity_store_get(store, instance->entity_id, &snapshot) == ESP_OK) {
                snprintf(pending->pending_state,
                         sizeof(pending->pending_state),
                         "%s",
                         snapshot.state);
            }
        }
    }

    ha_control_button_begin_loading(instance->control);

    if (pending) {
        pending->timeout_timer = lv_timer_create(pending_timeout_cb,
                                                 BUTTON_FEATURE_TIMEOUT_MS,
                                                 pending);
    }

    esp_err_t err = app_ha_call_entity_service("button", "press",
                                                instance->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "button.press for %s failed: %s",
                 instance->entity_id, esp_err_to_name(err));
        pending_release(pending);
        ha_control_button_finish_loading(instance->control, false);
    }
}

static esp_err_t button_feature_create(entity_feature_instance_t *instance)
{
    if (!instance || !instance->host || !instance->entity_id ||
        !instance->config ||
        instance->config->type != SCREEN_FEATURE_BUTTON) {
        return ESP_ERR_INVALID_ARG;
    }
    instance->control = ha_control_button_create(instance->host);
    if (!instance->control) {
        return ESP_ERR_NO_MEM;
    }
    ha_control_button_set_disabled(instance->control, true);
    ha_control_button_set_press_cb(instance->control, button_pressed,
                                   instance);
    return ESP_OK;
}

static void button_feature_update(entity_feature_instance_t *instance,
                                  ha_entity_store_t *store,
                                  const ha_entity_snapshot_t *snapshot)
{
    if (!instance || !instance->control) {
        return;
    }

    button_feature_pending_t *pending = pending_find(instance->control);
    if (pending && snapshot && pending->pending_state[0] &&
        ha_control_button_is_busy(instance->control) &&
        strcmp(pending->pending_state, snapshot->state) != 0) {
        ha_control_button_finish_loading(instance->control, true);
        pending_release(pending);
    }

    const bool available = store && snapshot &&
                           !entity_handler_snapshot_is_service_unavailable(snapshot);
    ha_control_button_set_disabled(instance->control, !available);
}

static void button_feature_apply_theme(entity_feature_instance_t *instance)
{
    if (instance && instance->control) {
        ha_control_button_refresh_theme(instance->control);
    }
}

const entity_feature_handler_t button_feature_handler = {
    .supports = button_feature_supports,
    .create = button_feature_create,
    .update = button_feature_update,
    .apply_theme = button_feature_apply_theme,
};
