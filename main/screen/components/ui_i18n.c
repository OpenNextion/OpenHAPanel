#include "ui_i18n.h"
#include "i18n.h"
#include <string.h>
#include <stdlib.h>

typedef struct label_binding {
    lv_obj_t *label;
    i18n_key_t key;
    struct label_binding *next;
} label_binding_t;

static label_binding_t *s_labels;

static void remove_binding(label_binding_t *binding)
{
    label_binding_t **link = &s_labels;
    while (*link && *link != binding) link = &(*link)->next;
    if (*link) {
        *link = binding->next;
        lv_free(binding);
    }
}

static void label_deleted(lv_event_t *event)
{
    remove_binding(lv_event_get_user_data(event));
}

void ui_i18n_label_set_text(lv_obj_t *label, const char *text)
{
    if (!label) return;
    label_binding_t *binding = s_labels;
    while (binding && binding->label != label) binding = binding->next;
    i18n_key_t key;
    if (i18n_resolve_key(text, &key)) {
        if (!binding) {
            binding = lv_malloc(sizeof(*binding));
            if (binding) {
                binding->label = label;
                binding->next = s_labels;
                s_labels = binding;
                if (!lv_obj_add_event_cb(label, label_deleted, LV_EVENT_DELETE, binding)) {
                    remove_binding(binding);
                    binding = NULL;
                }
            }
        }
        if (binding) binding->key = key;
        text = i18n_get(key);
    } else if (binding) {
        lv_obj_remove_event_cb_with_user_data(label, label_deleted, binding);
        remove_binding(binding);
    }
    const char *value = text ? text : "";
    char *normalized = NULL;
    /* Only combining marks U+0300..U+036F can match the compact table. */
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (*p == 0xCC || *p == 0xCD) {
            size_t capacity = strlen(value) + 1;
            normalized = malloc(capacity);
            if (normalized) {
                i18n_display_copy(normalized, capacity, value);
                value = normalized;
            }
            break;
        }
    }
    if (strcmp(lv_label_get_text(label), value) != 0)
        lv_label_set_text(label, value);
    free(normalized);
}

void ui_i18n_refresh_labels(void)
{
    for (label_binding_t *binding = s_labels; binding; binding = binding->next) {
        ui_i18n_label_set_text(binding->label, i18n_get(binding->key));
    }
}
