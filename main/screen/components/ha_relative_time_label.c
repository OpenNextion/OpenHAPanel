#include "ha_relative_time_label.h"

#include <string.h>

#include "entity_handlers/ha_datetime_format.h"

#define HA_RELATIVE_TIME_REFRESH_MS 1000
#define HA_RELATIVE_TIME_TEXT_MAX 64

typedef struct ha_relative_time_binding {
    lv_obj_t *label;
    time_t event_epoch;
    struct ha_relative_time_binding *next;
} ha_relative_time_binding_t;

static ha_relative_time_binding_t *s_bindings;
static lv_timer_t *s_refresh_timer;

static void ha_relative_time_timer_cb(lv_timer_t *timer);
static void ha_relative_time_label_delete_cb(lv_event_t *event);
static void ha_relative_time_update(ha_relative_time_binding_t *binding,
                                    time_t now_epoch);
static void ha_relative_time_remove_binding(
    ha_relative_time_binding_t *binding,
    bool remove_event_callback);

esp_err_t ha_relative_time_label_bind(lv_obj_t *label, time_t event_epoch)
{
    if (!label || event_epoch <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ha_relative_time_binding_t immediate = {
        .label = label,
        .event_epoch = event_epoch,
    };
    ha_relative_time_update(&immediate, time(NULL));

    for (ha_relative_time_binding_t *binding = s_bindings;
         binding;
         binding = binding->next) {
        if (binding->label == label) {
            binding->event_epoch = event_epoch;
            return ESP_OK;
        }
    }

    ha_relative_time_binding_t *binding = lv_malloc(sizeof(*binding));
    if (!binding) {
        return ESP_ERR_NO_MEM;
    }
    memset(binding, 0, sizeof(*binding));
    binding->label = label;
    binding->event_epoch = event_epoch;

    if (!s_refresh_timer) {
        s_refresh_timer = lv_timer_create(ha_relative_time_timer_cb,
                                          HA_RELATIVE_TIME_REFRESH_MS,
                                          NULL);
        if (!s_refresh_timer) {
            lv_free(binding);
            return ESP_ERR_NO_MEM;
        }
    }

    binding->next = s_bindings;
    s_bindings = binding;
    lv_obj_add_event_cb(label,
                        ha_relative_time_label_delete_cb,
                        LV_EVENT_DELETE,
                        binding);
    return ESP_OK;
}

void ha_relative_time_label_unbind(lv_obj_t *label)
{
    if (!label) {
        return;
    }

    for (ha_relative_time_binding_t *binding = s_bindings;
         binding;
         binding = binding->next) {
        if (binding->label == label) {
            ha_relative_time_remove_binding(binding, true);
            return;
        }
    }
}

static void ha_relative_time_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const time_t now_epoch = time(NULL);
    ha_relative_time_binding_t *binding = s_bindings;
    while (binding) {
        ha_relative_time_binding_t *next = binding->next;
        ha_relative_time_update(binding, now_epoch);
        binding = next;
    }
}

static void ha_relative_time_label_delete_cb(lv_event_t *event)
{
    ha_relative_time_binding_t *binding = lv_event_get_user_data(event);
    if (binding) {
        ha_relative_time_remove_binding(binding, false);
    }
}

static void ha_relative_time_update(ha_relative_time_binding_t *binding,
                                    time_t now_epoch)
{
    if (!binding || !binding->label) {
        return;
    }

    char text[HA_RELATIVE_TIME_TEXT_MAX] = {0};
    if (ha_datetime_format_relative_epoch(binding->event_epoch,
                                          now_epoch,
                                          text,
                                          sizeof(text)) != ESP_OK &&
        !text[0]) {
        return;
    }

    const char *current_text = lv_label_get_text(binding->label);
    if (!current_text || strcmp(current_text, text) != 0) {
        lv_label_set_text(binding->label, text);
    }
}

static void ha_relative_time_remove_binding(
    ha_relative_time_binding_t *binding,
    bool remove_event_callback)
{
    ha_relative_time_binding_t **link = &s_bindings;
    while (*link && *link != binding) {
        link = &(*link)->next;
    }
    if (!*link) {
        return;
    }

    *link = binding->next;
    if (remove_event_callback && binding->label) {
        lv_obj_remove_event_cb_with_user_data(binding->label,
                                              ha_relative_time_label_delete_cb,
                                              binding);
    }
    lv_free(binding);

    if (!s_bindings && s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }
}
