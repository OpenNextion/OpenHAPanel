#include "ha_entity_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ha_ws.h"

#define DEFAULT_MAX_ENTITIES 64
#define DEFAULT_QUEUE_SIZE 16
#define DEFAULT_WORKER_STACK_SIZE 6144
#define DEFAULT_WORKER_PRIORITY 5
#define MAX_LISTENERS 8
#define CONNECTION_POLL_MS 100
#define SYNC_RETRY_DELAY_MS 3000
#define INITIAL_SNAPSHOT_TIMEOUT_MS 15000

static const char *TAG = "ha_entity_store";

typedef struct {
    char *entity_id;
    char *state;
    char *last_changed;
    char *last_updated;
    cJSON *attributes;
    uint32_t revision;
    uint32_t updated_at_ms;
    uint32_t generation;
    uint32_t requested_sync_generation;
    uint32_t requested_fields;
    uint16_t ref_count;
    ha_entity_sync_status_t sync_status;
    bool occupied;
} entity_entry_t;

typedef enum {
    LISTENER_STORE_EVENT,
    LISTENER_ENTITY_CHANGED,
} listener_kind_t;

typedef struct {
    bool active;
    listener_kind_t kind;
    uint32_t generation;
    void *user_ctx;
    union {
        ha_entity_store_event_cb_t store_event;
        ha_entity_store_listener_t entity_changed;
    } callback;
} listener_slot_t;

typedef enum {
    MESSAGE_SYNC_REQUEST,
    MESSAGE_SUBSCRIBE_RESULT,
    MESSAGE_ENTITIES_UPDATE,
    MESSAGE_STOP,
} message_type_t;

typedef struct {
    message_type_t type;
    esp_err_t result;
    cJSON *json;
} store_message_t;

struct ha_entity_handle {
    ha_entity_store_t *store;
    size_t entry_index;
    uint32_t entry_generation;
    uint32_t requested_fields;
    struct ha_entity_handle *next;
};

struct ha_entity_listener_handle {
    ha_entity_store_t *store;
    size_t slot_index;
    uint32_t slot_generation;
};

struct ha_entity_store {
    ha_ws_client_t *ws_client;
    SemaphoreHandle_t lock;
    QueueHandle_t queue;
    TaskHandle_t worker_task;
    TaskHandle_t stop_waiter;
    entity_entry_t *entries;
    size_t max_entities;
    size_t entity_count;
    struct ha_entity_handle *handles;
    size_t live_handles;
    listener_slot_t listeners[MAX_LISTENERS];
    size_t live_listener_handles;
    uint32_t next_entry_generation;
    uint32_t next_listener_generation;
    uint32_t worker_stack_size;
    int worker_priority;
    bool prefer_external_memory;
    bool worker_stack_external;
    bool entries_external;
    ha_entity_store_status_t status;
    bool started;
    bool accept_messages;
    bool queue_overflow;

    /* The fields below are owned by the worker task. */
    bool last_authenticated;
    bool subscription_registered;
    bool subscribed;
    bool subscribing;
    bool sync_inflight;
    bool sync_pending;
    uint32_t sync_generation;
    uint64_t sync_started_ms;
    uint64_t retry_at_ms;
};

typedef struct {
    ha_entity_store_event_cb_t callback;
    void *user_ctx;
} event_listener_copy_t;

typedef struct {
    ha_entity_store_listener_t callback;
    void *user_ctx;
} entity_listener_copy_t;

typedef union {
    ha_entity_store_event_cb_t store_event;
    ha_entity_store_listener_t entity_changed;
} listener_callback_t;

static void store_worker(void *arg);
static void ws_entities_update_cb(ha_ws_client_t *client,
                                  int subscription_id,
                                  const cJSON *event,
                                  void *user_ctx);
static void ws_subscribe_result_cb(ha_ws_client_t *client,
                                   int id,
                                   esp_err_t err,
                                   const cJSON *result,
                                   const cJSON *error,
                                   void *user_ctx);

static char *store_strdup(const char *value)
{
    if (!value) {
        return NULL;
    }
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);
    if (copy) {
        memcpy(copy, value, length);
    }
    return copy;
}

static void copy_string(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }
    if (!source) {
        destination[0] = '\0';
        return;
    }
    strlcpy(destination, source, destination_size);
}

static const char *json_string(const cJSON *object, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static void free_entry(entity_entry_t *entry)
{
    free(entry->entity_id);
    free(entry->state);
    free(entry->last_changed);
    free(entry->last_updated);
    cJSON_Delete(entry->attributes);
    uint32_t generation = entry->generation;
    memset(entry, 0, sizeof(*entry));
    entry->generation = generation;
}

static entity_entry_t *find_entry_locked(ha_entity_store_t *store,
                                         const char *entity_id,
                                         size_t *index_out)
{
    for (size_t i = 0; i < store->max_entities; i++) {
        entity_entry_t *entry = &store->entries[i];
        if (entry->occupied && strcmp(entry->entity_id, entity_id) == 0) {
            if (index_out) {
                *index_out = i;
            }
            return entry;
        }
    }
    return NULL;
}

static bool has_entities(ha_entity_store_t *store)
{
    xSemaphoreTake(store->lock, portMAX_DELAY);
    bool result = store->entity_count != 0;
    xSemaphoreGive(store->lock);
    return result;
}

static size_t copy_event_listeners(ha_entity_store_t *store,
                                   event_listener_copy_t output[MAX_LISTENERS])
{
    size_t count = 0;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    for (size_t i = 0; i < MAX_LISTENERS; i++) {
        listener_slot_t *slot = &store->listeners[i];
        if (slot->active && slot->kind == LISTENER_STORE_EVENT && slot->callback.store_event) {
            output[count].callback = slot->callback.store_event;
            output[count].user_ctx = slot->user_ctx;
            count++;
        }
    }
    xSemaphoreGive(store->lock);
    return count;
}

static size_t copy_entity_listeners(ha_entity_store_t *store,
                                    entity_listener_copy_t output[MAX_LISTENERS])
{
    size_t count = 0;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    for (size_t i = 0; i < MAX_LISTENERS; i++) {
        listener_slot_t *slot = &store->listeners[i];
        if (slot->active && slot->kind == LISTENER_ENTITY_CHANGED &&
            slot->callback.entity_changed) {
            output[count].callback = slot->callback.entity_changed;
            output[count].user_ctx = slot->user_ctx;
            count++;
        }
    }
    xSemaphoreGive(store->lock);
    return count;
}

static void fire_store_event(ha_entity_store_t *store,
                             ha_entity_store_event_type_t type,
                             esp_err_t result,
                             size_t matched,
                             size_t changed)
{
    ha_entity_store_event_t event = {
        .type = type,
        .result = result,
        .sync_generation = store->sync_generation,
        .matched_entities = matched,
        .changed_entities = changed,
        .status = ha_entity_store_get_status(store),
    };
    event_listener_copy_t listeners[MAX_LISTENERS];
    size_t count = copy_event_listeners(store, listeners);
    for (size_t i = 0; i < count; i++) {
        listeners[i].callback(store, &event, listeners[i].user_ctx);
    }
}

static void set_status(ha_entity_store_t *store, ha_entity_store_status_t status)
{
    xSemaphoreTake(store->lock, portMAX_DELAY);
    bool changed = store->status != status;
    store->status = status;
    xSemaphoreGive(store->lock);
    if (changed) {
        fire_store_event(store, HA_ENTITY_STORE_EVENT_STATUS_CHANGED, ESP_OK, 0, 0);
    }
}

static void fire_entity_changed(ha_entity_store_t *store,
                                const char *entity_id,
                                uint32_t revision,
                                uint32_t changes)
{
    if (changes == 0) {
        return;
    }
    ha_entity_changed_event_t event = {
        .revision = revision,
        .changes = changes,
    };
    copy_string(event.entity_id, sizeof(event.entity_id), entity_id);
    entity_listener_copy_t listeners[MAX_LISTENERS];
    size_t count = copy_entity_listeners(store, listeners);
    for (size_t i = 0; i < count; i++) {
        listeners[i].callback(store, &event, listeners[i].user_ctx);
    }
}

static bool enqueue_from_ws(ha_entity_store_t *store, store_message_t *message)
{
    xSemaphoreTake(store->lock, portMAX_DELAY);
    bool accepted = store->accept_messages;
    xSemaphoreGive(store->lock);
    if (!accepted || xQueueSend(store->queue, message, 0) != pdTRUE) {
        cJSON_Delete(message->json);
        if (accepted) {
            xSemaphoreTake(store->lock, portMAX_DELAY);
            store->queue_overflow = true;
            xSemaphoreGive(store->lock);
            ESP_LOGE(TAG, "Store queue full, dropping WebSocket message");
        }
        return false;
    }
    return true;
}

static bool consume_queue_overflow(ha_entity_store_t *store)
{
    xSemaphoreTake(store->lock, portMAX_DELAY);
    bool overflow = store->queue_overflow;
    store->queue_overflow = false;
    xSemaphoreGive(store->lock);
    return overflow;
}

static void ws_entities_update_cb(ha_ws_client_t *client,
                                  int subscription_id,
                                  const cJSON *event,
                                  void *user_ctx)
{
    (void)client;
    (void)subscription_id;
    ha_entity_store_t *store = user_ctx;
    store_message_t message = {
        .type = MESSAGE_ENTITIES_UPDATE,
        .result = ESP_OK,
        .json = cJSON_Duplicate(event, true),
    };
    if (!message.json) {
        ESP_LOGE(TAG, "Failed to copy subscribe_entities update");
        return;
    }
    enqueue_from_ws(store, &message);
}

static void ws_subscribe_result_cb(ha_ws_client_t *client,
                                   int id,
                                   esp_err_t err,
                                   const cJSON *result,
                                   const cJSON *error,
                                   void *user_ctx)
{
    (void)client;
    (void)id;
    (void)result;
    (void)error;
    ha_entity_store_t *store = user_ctx;
    store_message_t message = {
        .type = MESSAGE_SUBSCRIBE_RESULT,
        .result = err,
    };
    enqueue_from_ws(store, &message);
}

static bool apply_state_object(ha_entity_store_t *store,
                               const cJSON *state_object,
                               uint32_t *changes_out)
{
    const char *entity_id = json_string(state_object, "entity_id");
    const char *state_value = json_string(state_object, "state");
    const char *last_changed = json_string(state_object, "last_changed");
    const char *last_updated = json_string(state_object, "last_updated");
    const cJSON *attributes = cJSON_GetObjectItemCaseSensitive(state_object, "attributes");
    if (!entity_id || !state_value) {
        return false;
    }

    xSemaphoreTake(store->lock, portMAX_DELAY);
    bool wanted = find_entry_locked(store, entity_id, NULL) != NULL;
    xSemaphoreGive(store->lock);
    if (!wanted) {
        return false;
    }

    char *state_copy = store_strdup(state_value);
    char *changed_copy = store_strdup(last_changed);
    char *updated_copy = store_strdup(last_updated);
    cJSON *attributes_copy = cJSON_IsObject(attributes) ? cJSON_Duplicate(attributes, true) : NULL;
    if (!state_copy || (last_changed && !changed_copy) || (last_updated && !updated_copy) ||
        (cJSON_IsObject(attributes) && !attributes_copy)) {
        free(state_copy);
        free(changed_copy);
        free(updated_copy);
        cJSON_Delete(attributes_copy);
        return false;
    }

    uint32_t changes = 0;
    uint32_t revision = 0;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    if (!entry) {
        xSemaphoreGive(store->lock);
        free(state_copy);
        free(changed_copy);
        free(updated_copy);
        cJSON_Delete(attributes_copy);
        return false;
    }

    if (!entry->state || strcmp(entry->state, state_value) != 0) {
        changes |= HA_ENTITY_CHANGE_STATE;
    }
    if ((!entry->attributes && attributes_copy) || (entry->attributes && !attributes_copy) ||
        (entry->attributes && attributes_copy &&
         !cJSON_Compare(entry->attributes, attributes_copy, true))) {
        changes |= HA_ENTITY_CHANGE_ATTRIBUTES;
    }
    if (entry->sync_status != HA_ENTITY_SYNC_READY) {
        changes |= HA_ENTITY_CHANGE_AVAILABILITY;
    }
    if (entry->sync_status == HA_ENTITY_SYNC_UNKNOWN) {
        changes |= HA_ENTITY_CHANGE_ADDED;
    }

    free(entry->state);
    free(entry->last_changed);
    free(entry->last_updated);
    cJSON_Delete(entry->attributes);
    entry->state = state_copy;
    entry->last_changed = changed_copy;
    entry->last_updated = updated_copy;
    entry->attributes = attributes_copy;
    entry->sync_status = HA_ENTITY_SYNC_READY;
    entry->updated_at_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (changes) {
        entry->revision++;
    }
    revision = entry->revision;
    xSemaphoreGive(store->lock);

    if (changes_out) {
        *changes_out = changes;
    }
    fire_entity_changed(store, entity_id, revision, changes);
    return true;
}

static bool mark_entity_removed(ha_entity_store_t *store, const char *entity_id)
{
    uint32_t revision = 0;
    uint32_t changes = 0;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    if (entry && entry->sync_status != HA_ENTITY_SYNC_REMOVED) {
        entry->sync_status = HA_ENTITY_SYNC_REMOVED;
        entry->revision++;
        revision = entry->revision;
        changes = HA_ENTITY_CHANGE_AVAILABILITY | HA_ENTITY_CHANGE_REMOVED;
    }
    xSemaphoreGive(store->lock);
    fire_entity_changed(store, entity_id, revision, changes);
    return changes != 0;
}

static void mark_all_stale(ha_entity_store_t *store)
{
    xSemaphoreTake(store->lock, portMAX_DELAY);
    for (size_t i = 0; i < store->max_entities; i++) {
        entity_entry_t *entry = &store->entries[i];
        if (entry->occupied && entry->sync_status != HA_ENTITY_SYNC_UNKNOWN &&
            entry->sync_status != HA_ENTITY_SYNC_REMOVED) {
            entry->sync_status = HA_ENTITY_SYNC_STALE;
        }
    }
    xSemaphoreGive(store->lock);
}

static bool format_compact_timestamp(const cJSON *value,
                                     char output[HA_ENTITY_STORE_DATETIME_MAX])
{
    if (!cJSON_IsNumber(value) || value->valuedouble <= 0.0) {
        return false;
    }
    time_t seconds = (time_t)value->valuedouble;
    struct tm utc = {0};
    if (!gmtime_r(&seconds, &utc)) {
        return false;
    }
    return strftime(output,
                    HA_ENTITY_STORE_DATETIME_MAX,
                    "%Y-%m-%dT%H:%M:%S+00:00",
                    &utc) != 0;
}

static cJSON *state_object_from_compact(const char *entity_id, const cJSON *compact)
{
    const char *state = json_string(compact, "s");
    const cJSON *attributes = cJSON_GetObjectItemCaseSensitive(compact, "a");
    const cJSON *last_changed = cJSON_GetObjectItemCaseSensitive(compact, "lc");
    const cJSON *last_updated = cJSON_GetObjectItemCaseSensitive(compact, "lu");
    char changed_buffer[HA_ENTITY_STORE_DATETIME_MAX] = {0};
    char updated_buffer[HA_ENTITY_STORE_DATETIME_MAX] = {0};
    if (!entity_id || !state || !format_compact_timestamp(last_changed, changed_buffer)) {
        return NULL;
    }
    if (!format_compact_timestamp(last_updated, updated_buffer)) {
        copy_string(updated_buffer, sizeof(updated_buffer), changed_buffer);
    }

    cJSON *full = cJSON_CreateObject();
    cJSON *attributes_copy = cJSON_IsObject(attributes)
                                 ? cJSON_Duplicate(attributes, true)
                                 : cJSON_CreateObject();
    if (!full || !attributes_copy ||
        !cJSON_AddStringToObject(full, "entity_id", entity_id) ||
        !cJSON_AddStringToObject(full, "state", state) ||
        !cJSON_AddStringToObject(full, "last_changed", changed_buffer) ||
        !cJSON_AddStringToObject(full, "last_updated", updated_buffer) ||
        !cJSON_AddItemToObject(full, "attributes", attributes_copy)) {
        cJSON_Delete(attributes_copy);
        cJSON_Delete(full);
        return NULL;
    }
    return full;
}

static cJSON *state_object_from_entry(ha_entity_store_t *store, const char *entity_id)
{
    cJSON *full = NULL;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    if (entry && entry->state) {
        full = cJSON_CreateObject();
        cJSON *attributes = entry->attributes ? cJSON_Duplicate(entry->attributes, true)
                                              : cJSON_CreateObject();
        if (!full || !attributes ||
            !cJSON_AddStringToObject(full, "entity_id", entity_id) ||
            !cJSON_AddStringToObject(full, "state", entry->state) ||
            (entry->last_changed &&
             !cJSON_AddStringToObject(full, "last_changed", entry->last_changed)) ||
            (entry->last_updated &&
             !cJSON_AddStringToObject(full, "last_updated", entry->last_updated)) ||
            !cJSON_AddItemToObject(full, "attributes", attributes)) {
            cJSON_Delete(attributes);
            cJSON_Delete(full);
            full = NULL;
        }
    }
    xSemaphoreGive(store->lock);
    return full;
}

static bool replace_string(cJSON *object, const char *name, const char *value)
{
    cJSON *string = cJSON_CreateString(value);
    if (!string) {
        return false;
    }
    if (cJSON_HasObjectItem(object, name)) {
        if (!cJSON_ReplaceItemInObjectCaseSensitive(object, name, string)) {
            cJSON_Delete(string);
            return false;
        }
        return true;
    }
    if (!cJSON_AddItemToObject(object, name, string)) {
        cJSON_Delete(string);
        return false;
    }
    return true;
}

static bool apply_compact_diff(ha_entity_store_t *store,
                               const char *entity_id,
                               const cJSON *diff,
                               uint32_t *changes_out)
{
    cJSON *full = state_object_from_entry(store, entity_id);
    if (!full || !cJSON_IsObject(diff)) {
        cJSON_Delete(full);
        return false;
    }

    const cJSON *added = cJSON_GetObjectItemCaseSensitive(diff, "+");
    const cJSON *removed = cJSON_GetObjectItemCaseSensitive(diff, "-");
    bool valid = true;
    if (cJSON_IsObject(added)) {
        const char *state = json_string(added, "s");
        if (state) {
            valid = replace_string(full, "state", state);
        }

        char timestamp[HA_ENTITY_STORE_DATETIME_MAX] = {0};
        const cJSON *last_changed = cJSON_GetObjectItemCaseSensitive(added, "lc");
        const cJSON *last_updated = cJSON_GetObjectItemCaseSensitive(added, "lu");
        if (valid && format_compact_timestamp(last_changed, timestamp)) {
            valid = replace_string(full, "last_changed", timestamp) &&
                    replace_string(full, "last_updated", timestamp);
        } else if (valid && format_compact_timestamp(last_updated, timestamp)) {
            valid = replace_string(full, "last_updated", timestamp);
        }

        const cJSON *attribute_updates = cJSON_GetObjectItemCaseSensitive(added, "a");
        cJSON *attributes = cJSON_GetObjectItemCaseSensitive(full, "attributes");
        const cJSON *attribute = NULL;
        cJSON_ArrayForEach(attribute, attribute_updates) {
            if (!attribute->string) {
                valid = false;
                break;
            }
            cJSON *copy = cJSON_Duplicate(attribute, true);
            if (!copy) {
                valid = false;
                break;
            }
            if (cJSON_HasObjectItem(attributes, attribute->string)) {
                if (!cJSON_ReplaceItemInObjectCaseSensitive(attributes,
                                                            attribute->string,
                                                            copy)) {
                    cJSON_Delete(copy);
                    valid = false;
                    break;
                }
            } else if (!cJSON_AddItemToObject(attributes, attribute->string, copy)) {
                cJSON_Delete(copy);
                valid = false;
                break;
            }
        }
    }

    const cJSON *attribute_removals = cJSON_IsObject(removed)
                                          ? cJSON_GetObjectItemCaseSensitive(removed, "a")
                                          : NULL;
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(full, "attributes");
    const cJSON *attribute_name = NULL;
    cJSON_ArrayForEach(attribute_name, attribute_removals) {
        if (cJSON_IsString(attribute_name) && attribute_name->valuestring) {
            cJSON_DeleteItemFromObjectCaseSensitive(attributes, attribute_name->valuestring);
        }
    }

    bool applied = valid && apply_state_object(store, full, changes_out);
    cJSON_Delete(full);
    return applied;
}

static esp_err_t process_entities_update(ha_entity_store_t *store,
                                         const cJSON *updates,
                                         bool initial,
                                         size_t *matched_out,
                                         size_t *changed_out)
{
    if (!cJSON_IsObject(updates)) {
        return HA_WS_ERR_INVALID_RESPONSE;
    }
    uint32_t *found_generation = initial
                                     ? calloc(store->max_entities, sizeof(*found_generation))
                                     : NULL;
    if (initial && !found_generation) {
        return ESP_ERR_NO_MEM;
    }

    size_t matched = 0;
    size_t changed = 0;
    const cJSON *added = cJSON_GetObjectItemCaseSensitive(updates, "a");
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, added) {
        const char *entity_id = item->string;
        size_t index = 0;
        uint32_t generation = 0;
        xSemaphoreTake(store->lock, portMAX_DELAY);
        entity_entry_t *entry = entity_id ? find_entry_locked(store, entity_id, &index) : NULL;
        if (entry) {
            generation = entry->generation;
        }
        xSemaphoreGive(store->lock);
        if (!entry) {
            continue;
        }

        cJSON *full = state_object_from_compact(entity_id, item);
        uint32_t entity_changes = 0;
        if (full && apply_state_object(store, full, &entity_changes)) {
            if (found_generation) {
                found_generation[index] = generation;
            }
            matched++;
            changed += entity_changes != 0;
        }
        cJSON_Delete(full);
    }

    const cJSON *removed = cJSON_GetObjectItemCaseSensitive(updates, "r");
    cJSON_ArrayForEach(item, removed) {
        if (cJSON_IsString(item) && item->valuestring &&
            mark_entity_removed(store, item->valuestring)) {
            changed++;
        }
    }

    const cJSON *changed_entities = cJSON_GetObjectItemCaseSensitive(updates, "c");
    cJSON_ArrayForEach(item, changed_entities) {
        uint32_t entity_changes = 0;
        if (item->string && apply_compact_diff(store,
                                               item->string,
                                               item,
                                               &entity_changes)) {
            matched++;
            changed += entity_changes != 0;
        }
    }

    if (initial) {
        for (size_t i = 0; i < store->max_entities; i++) {
            char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX] = {0};
            xSemaphoreTake(store->lock, portMAX_DELAY);
            entity_entry_t *entry = &store->entries[i];
            if (entry->occupied &&
                entry->requested_sync_generation == store->sync_generation &&
                found_generation[i] != entry->generation) {
                copy_string(entity_id, sizeof(entity_id), entry->entity_id);
            }
            xSemaphoreGive(store->lock);
            if (entity_id[0] && mark_entity_removed(store, entity_id)) {
                changed++;
            }
        }
    }

    free(found_generation);
    if (matched_out) {
        *matched_out = matched;
    }
    if (changed_out) {
        *changed_out = changed;
    }
    return ESP_OK;
}

static esp_err_t begin_subscribe(ha_entity_store_t *store)
{
    char **entity_ids = calloc(store->max_entities, sizeof(*entity_ids));
    if (!entity_ids) {
        return ESP_ERR_NO_MEM;
    }
    size_t entity_id_count = 0;
    size_t expected_count = 0;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    expected_count = store->entity_count;
    for (size_t i = 0; i < store->max_entities; i++) {
        entity_entry_t *entry = &store->entries[i];
        if (!entry->occupied) {
            continue;
        }
        entity_ids[entity_id_count] = store_strdup(entry->entity_id);
        if (!entity_ids[entity_id_count]) {
            break;
        }
        entity_id_count++;
    }
    xSemaphoreGive(store->lock);

    if (entity_id_count != expected_count) {
        for (size_t i = 0; i < entity_id_count; i++) {
            free(entity_ids[i]);
        }
        free(entity_ids);
        return ESP_ERR_NO_MEM;
    }

    store->subscribing = true;
    ESP_LOGI(TAG, "Subscribing to %u Home Assistant entities", (unsigned)entity_id_count);
    esp_err_t err = ha_ws_subscribe_entities(store->ws_client,
                                             (const char *const *)entity_ids,
                                             entity_id_count,
                                             ws_entities_update_cb,
                                             ws_subscribe_result_cb,
                                             store,
                                             NULL);
    for (size_t i = 0; i < entity_id_count; i++) {
        free(entity_ids[i]);
    }
    free(entity_ids);

    if (err != ESP_OK) {
        store->subscribing = false;
    } else {
        store->subscription_registered = true;
    }
    return err;
}

static void begin_sync(ha_entity_store_t *store)
{
    store->sync_pending = false;
    store->retry_at_ms = 0;
    if (store->subscription_registered) {
        ha_ws_unsubscribe_events_by_callback(store->ws_client,
                                             ws_entities_update_cb,
                                             store);
        store->subscription_registered = false;
        store->subscribed = false;
        store->subscribing = false;
    }
    if (!has_entities(store)) {
        store->sync_generation++;
        set_status(store, HA_ENTITY_STORE_READY);
        fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_STARTED, ESP_OK, 0, 0);
        fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_SUCCEEDED, ESP_OK, 0, 0);
        return;
    }

    store->sync_generation++;
    store->sync_inflight = true;
    store->sync_started_ms = (uint64_t)esp_timer_get_time() / 1000;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    for (size_t i = 0; i < store->max_entities; i++) {
        entity_entry_t *entry = &store->entries[i];
        if (entry->occupied) {
            entry->requested_sync_generation = store->sync_generation;
        }
    }
    xSemaphoreGive(store->lock);
    set_status(store, HA_ENTITY_STORE_SYNCING);
    fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_STARTED, ESP_OK, 0, 0);
    esp_err_t err = begin_subscribe(store);
    if (err != ESP_OK) {
        store->sync_inflight = false;
        mark_all_stale(store);
        if (err == HA_WS_ERR_NOT_AUTHENTICATED) {
            store->sync_pending = true;
            set_status(store, HA_ENTITY_STORE_OFFLINE);
        } else {
            store->sync_pending = true;
            store->retry_at_ms = (uint64_t)esp_timer_get_time() / 1000 + SYNC_RETRY_DELAY_MS;
            set_status(store, HA_ENTITY_STORE_DEGRADED);
        }
        ESP_LOGW(TAG, "Entity subscription failed to start: %s; retrying", esp_err_to_name(err));
        fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_FAILED, err, 0, 0);
    }
}

static void drive_worker(ha_entity_store_t *store, bool authenticated)
{
    if (!authenticated || !store->sync_pending || store->sync_inflight ||
        store->subscribing) {
        return;
    }
    if ((uint64_t)esp_timer_get_time() / 1000 < store->retry_at_ms) {
        return;
    }
    begin_sync(store);
}

static void handle_message(ha_entity_store_t *store,
                           store_message_t *message,
                           bool *should_stop)
{
    switch (message->type) {
    case MESSAGE_SYNC_REQUEST:
        store->sync_pending = true;
        break;
    case MESSAGE_SUBSCRIBE_RESULT:
        store->subscribing = false;
        if (message->result == ESP_OK) {
            store->subscribed = true;
            /* begin_sync() already consumed the request which started this
             * subscription. Keep sync_pending intact here: another entity may
             * have been acquired while the subscription was in flight. Once
             * its initial snapshot completes, drive_worker() will rebuild the
             * subscription with the latest entity set. */
            if (!store->sync_inflight) {
                store->sync_generation++;
                store->sync_inflight = true;
                store->sync_started_ms = (uint64_t)esp_timer_get_time() / 1000;
                xSemaphoreTake(store->lock, portMAX_DELAY);
                for (size_t i = 0; i < store->max_entities; i++) {
                    entity_entry_t *entry = &store->entries[i];
                    if (entry->occupied) {
                        entry->requested_sync_generation = store->sync_generation;
                    }
                }
                xSemaphoreGive(store->lock);
                set_status(store, HA_ENTITY_STORE_SYNCING);
                fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_STARTED, ESP_OK, 0, 0);
            }
        } else if (message->result == ESP_ERR_INVALID_STATE &&
                   store->subscription_registered) {
            /* ha_ws rejects pending callbacks on disconnect, but keeps the
             * subscription so it can be restored after authentication. */
            store->subscribed = false;
            store->subscribing = true;
            store->sync_inflight = false;
            store->sync_pending = true;
            set_status(store, HA_ENTITY_STORE_OFFLINE);
        } else {
            ha_ws_unsubscribe_events_by_callback(store->ws_client,
                                                 ws_entities_update_cb,
                                                 store);
            store->subscription_registered = false;
            store->sync_inflight = false;
            store->subscribed = false;
            mark_all_stale(store);
            if (ha_ws_is_authenticated(store->ws_client)) {
                store->sync_pending = true;
                store->retry_at_ms = (uint64_t)esp_timer_get_time() / 1000 + SYNC_RETRY_DELAY_MS;
                set_status(store, HA_ENTITY_STORE_DEGRADED);
            } else {
                store->sync_pending = true;
                set_status(store, HA_ENTITY_STORE_OFFLINE);
            }
            ESP_LOGW(TAG, "Entity subscription rejected: %s; retrying",
                     esp_err_to_name(message->result));
            fire_store_event(store,
                             HA_ENTITY_STORE_EVENT_SYNC_FAILED,
                             message->result,
                             0,
                             0);
        }
        break;
    case MESSAGE_ENTITIES_UPDATE: {
        bool initial = store->sync_inflight;
        size_t matched = 0;
        size_t changed = 0;
        esp_err_t err = process_entities_update(store,
                                                message->json,
                                                initial,
                                                &matched,
                                                &changed);
        if (err != ESP_OK) {
            store->sync_inflight = false;
            ha_ws_unsubscribe_events_by_callback(store->ws_client,
                                                 ws_entities_update_cb,
                                                 store);
            store->subscription_registered = false;
            store->subscribed = false;
            store->subscribing = false;
            store->sync_pending = true;
            store->retry_at_ms = (uint64_t)esp_timer_get_time() / 1000 + SYNC_RETRY_DELAY_MS;
            mark_all_stale(store);
            set_status(store, HA_ENTITY_STORE_DEGRADED);
            ESP_LOGW(TAG, "Invalid entity update: %s; retrying", esp_err_to_name(err));
            fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_FAILED, err, 0, 0);
        } else if (initial) {
            store->sync_inflight = false;
            store->retry_at_ms = 0;
            set_status(store, HA_ENTITY_STORE_READY);
            ESP_LOGI(TAG, "Initial entity snapshot ready: matched=%u changed=%u",
                     (unsigned)matched, (unsigned)changed);
            fire_store_event(store,
                             HA_ENTITY_STORE_EVENT_SYNC_SUCCEEDED,
                             ESP_OK,
                             matched,
                             changed);
        }
        break;
    }
    case MESSAGE_STOP:
        *should_stop = true;
        break;
    }
    cJSON_Delete(message->json);
    message->json = NULL;
}

static void store_worker(void *arg)
{
    ha_entity_store_t *store = arg;
    bool should_stop = false;
    while (!should_stop) {
        store_message_t message = {0};
        if (xQueueReceive(store->queue,
                          &message,
                          pdMS_TO_TICKS(CONNECTION_POLL_MS)) == pdTRUE) {
            handle_message(store, &message, &should_stop);
        }
        if (should_stop) {
            break;
        }

        if (consume_queue_overflow(store)) {
            store_message_t dropped = {0};
            while (xQueueReceive(store->queue, &dropped, 0) == pdTRUE) {
                if (dropped.type == MESSAGE_STOP) {
                    should_stop = true;
                }
                cJSON_Delete(dropped.json);
            }
            if (should_stop) {
                break;
            }
            ha_ws_unsubscribe_events_by_callback(store->ws_client,
                                                 ws_entities_update_cb,
                                                 store);
            ha_ws_cancel_pending_by_context(store->ws_client, store);
            store->subscription_registered = false;
            store->subscribed = false;
            store->subscribing = false;
            store->sync_inflight = false;
            store->sync_pending = true;
            mark_all_stale(store);
            set_status(store, HA_ENTITY_STORE_DEGRADED);
            fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_FAILED, ESP_ERR_TIMEOUT, 0, 0);
        }

        bool authenticated = ha_ws_is_authenticated(store->ws_client);
        if (authenticated != store->last_authenticated) {
            store->last_authenticated = authenticated;
            if (authenticated) {
                if (has_entities(store)) {
                    store->sync_pending = true;
                } else {
                    set_status(store, HA_ENTITY_STORE_READY);
                }
            } else {
                store->subscribed = false;
                store->subscribing = store->subscription_registered;
                store->sync_inflight = false;
                mark_all_stale(store);
                set_status(store, HA_ENTITY_STORE_OFFLINE);
            }
        }
        if (authenticated && store->sync_inflight &&
            (uint64_t)esp_timer_get_time() / 1000 - store->sync_started_ms >=
                INITIAL_SNAPSHOT_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Initial entity snapshot timed out; resubscribing");
            ha_ws_unsubscribe_events_by_callback(store->ws_client,
                                                 ws_entities_update_cb,
                                                 store);
            store->subscription_registered = false;
            store->subscribed = false;
            store->subscribing = false;
            store->sync_inflight = false;
            store->sync_pending = true;
            store->retry_at_ms = (uint64_t)esp_timer_get_time() / 1000 + SYNC_RETRY_DELAY_MS;
            mark_all_stale(store);
            set_status(store, HA_ENTITY_STORE_DEGRADED);
            fire_store_event(store, HA_ENTITY_STORE_EVENT_SYNC_FAILED, ESP_ERR_TIMEOUT, 0, 0);
        }
        drive_worker(store, authenticated);
    }

    ha_ws_unsubscribe_events_by_callback(store->ws_client,
                                         ws_entities_update_cb,
                                         store);
    ha_ws_cancel_pending_by_context(store->ws_client, store);

    store_message_t pending = {0};
    while (xQueueReceive(store->queue, &pending, 0) == pdTRUE) {
        cJSON_Delete(pending.json);
    }

    mark_all_stale(store);
    set_status(store, HA_ENTITY_STORE_STOPPED);
    xSemaphoreTake(store->lock, portMAX_DELAY);
    TaskHandle_t waiter = store->stop_waiter;
    bool worker_stack_external = store->worker_stack_external;
    store->worker_task = NULL;
    store->worker_stack_external = false;
    store->started = false;
    xSemaphoreGive(store->lock);
    if (waiter) {
        xTaskNotifyGive(waiter);
    }
    if (worker_stack_external) {
        vTaskDeleteWithCaps(NULL);
    }
    else {
        vTaskDelete(NULL);
    }
}

esp_err_t ha_entity_store_create(const ha_entity_store_config_t *config,
                                 ha_entity_store_t **out_store)
{
    if (!config || !config->ws_client || !out_store) {
        return ESP_ERR_INVALID_ARG;
    }

    ha_entity_store_t *store = calloc(1, sizeof(*store));
    if (!store) {
        return ESP_ERR_NO_MEM;
    }
    store->ws_client = config->ws_client;
    store->max_entities = config->max_entities ? config->max_entities : DEFAULT_MAX_ENTITIES;
    store->worker_stack_size = config->worker_stack_size ? config->worker_stack_size
                                                         : DEFAULT_WORKER_STACK_SIZE;
    store->worker_priority = config->worker_priority ? config->worker_priority
                                                     : DEFAULT_WORKER_PRIORITY;
    store->prefer_external_memory = config->prefer_external_memory;
    store->status = HA_ENTITY_STORE_STOPPED;
    if (store->prefer_external_memory) {
        store->entries = heap_caps_calloc(store->max_entities,
                                          sizeof(*store->entries),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        store->entries_external = store->entries != NULL;
    }
    if (!store->entries) {
        store->entries = calloc(store->max_entities, sizeof(*store->entries));
        if (store->prefer_external_memory && store->entries) {
            ESP_LOGW(TAG, "PSRAM entity table allocation failed; using internal RAM");
        }
    }
    store->lock = xSemaphoreCreateMutex();
    store->queue = xQueueCreate(config->event_queue_size ? config->event_queue_size
                                                         : DEFAULT_QUEUE_SIZE,
                                sizeof(store_message_t));
    if (!store->entries || !store->lock || !store->queue) {
        if (store->queue) {
            vQueueDelete(store->queue);
        }
        if (store->lock) {
            vSemaphoreDelete(store->lock);
        }
        free(store->entries);
        free(store);
        return ESP_ERR_NO_MEM;
    }
    *out_store = store;
    return ESP_OK;
}

esp_err_t ha_entity_store_start(ha_entity_store_t *store)
{
    if (!store) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    if (store->started) {
        xSemaphoreGive(store->lock);
        return ESP_OK;
    }
    store->accept_messages = true;
    store->queue_overflow = false;
    store->stop_waiter = NULL;
    store->last_authenticated = false;
    store->subscription_registered = false;
    store->subscribed = false;
    store->subscribing = false;
    store->sync_inflight = false;
    store->sync_pending = store->entity_count != 0;
    BaseType_t created = pdFAIL;
    if (store->prefer_external_memory) {
        created = xTaskCreateWithCaps(store_worker,
                                      "ha_entity_store",
                                      store->worker_stack_size,
                                      store,
                                      store->worker_priority,
                                      &store->worker_task,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        store->worker_stack_external = created == pdPASS;
        if (created != pdPASS) {
            ESP_LOGW(TAG, "PSRAM worker stack allocation failed; using internal RAM");
        }
    }
    if (created != pdPASS) {
        created = xTaskCreate(store_worker,
                              "ha_entity_store",
                              store->worker_stack_size,
                              store,
                              store->worker_priority,
                              &store->worker_task);
        store->worker_stack_external = false;
    }
    if (created != pdPASS) {
        store->accept_messages = false;
        store->worker_task = NULL;
        xSemaphoreGive(store->lock);
        return ESP_ERR_NO_MEM;
    }
    store->started = true;
    store->status = HA_ENTITY_STORE_OFFLINE;
    xSemaphoreGive(store->lock);
    ESP_LOGI(TAG,
             "Worker started: stack=%u memory=%s entries=%s",
             (unsigned)store->worker_stack_size,
             store->worker_stack_external ? "PSRAM" : "internal",
             store->entries_external ? "PSRAM" : "internal");
    return ESP_OK;
}

esp_err_t ha_entity_store_stop(ha_entity_store_t *store)
{
    if (!store) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    if (!store->started) {
        xSemaphoreGive(store->lock);
        return ESP_OK;
    }
    if (xTaskGetCurrentTaskHandle() == store->worker_task) {
        xSemaphoreGive(store->lock);
        return ESP_ERR_INVALID_STATE;
    }
    store->accept_messages = false;
    store->stop_waiter = xTaskGetCurrentTaskHandle();
    xSemaphoreGive(store->lock);

    store_message_t message = {.type = MESSAGE_STOP};
    if (xQueueSendToFront(store->queue, &message, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t ha_entity_store_destroy(ha_entity_store_t *store)
{
    if (!store) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    bool has_live_handles = store->live_handles != 0 || store->live_listener_handles != 0;
    xSemaphoreGive(store->lock);
    if (has_live_handles) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = ha_entity_store_stop(store);
    if (err != ESP_OK) {
        return err;
    }
    for (size_t i = 0; i < store->max_entities; i++) {
        free_entry(&store->entries[i]);
    }
    vQueueDelete(store->queue);
    vSemaphoreDelete(store->lock);
    free(store->entries);
    free(store);
    return ESP_OK;
}

esp_err_t ha_entity_store_sync(ha_entity_store_t *store)
{
    if (!store) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    bool started = store->started && store->accept_messages;
    xSemaphoreGive(store->lock);
    if (!started) {
        return ESP_ERR_INVALID_STATE;
    }
    store_message_t message = {.type = MESSAGE_SYNC_REQUEST};
    return xQueueSend(store->queue, &message, pdMS_TO_TICKS(100)) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

esp_err_t ha_entity_store_acquire(ha_entity_store_t *store,
                                  const char *entity_id,
                                  uint32_t requested_fields,
                                  ha_entity_handle_t **out_handle)
{
    if (!store || !entity_id || !entity_id[0] || !out_handle ||
        strlen(entity_id) >= HA_ENTITY_STORE_ENTITY_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    ha_entity_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    if (requested_fields == 0) {
        requested_fields = HA_ENTITY_FIELD_ALL;
    }

    xSemaphoreTake(store->lock, portMAX_DELAY);
    size_t index = 0;
    entity_entry_t *entry = find_entry_locked(store, entity_id, &index);
    if (!entry) {
        for (index = 0; index < store->max_entities; index++) {
            if (!store->entries[index].occupied) {
                entry = &store->entries[index];
                break;
            }
        }
        /* During a screen reload, zero-reference entries retain their last
         * snapshot until the rebuilt views acquire their entities. If the
         * temporary cache fills the store, evict only one of those entries;
         * live UI consumers are never displaced. */
        if (!entry) {
            for (index = 0; index < store->max_entities; index++) {
                if (store->entries[index].occupied &&
                    store->entries[index].ref_count == 0) {
                    entry = &store->entries[index];
                    free_entry(entry);
                    store->entity_count--;
                    break;
                }
            }
        }
        if (!entry) {
            xSemaphoreGive(store->lock);
            free(handle);
            return ESP_ERR_NO_MEM;
        }
        entry->entity_id = store_strdup(entity_id);
        if (!entry->entity_id) {
            xSemaphoreGive(store->lock);
            free(handle);
            return ESP_ERR_NO_MEM;
        }
        entry->occupied = true;
        entry->sync_status = HA_ENTITY_SYNC_UNKNOWN;
        entry->generation = ++store->next_entry_generation;
        store->entity_count++;
    }
    entry->ref_count++;
    entry->requested_fields |= requested_fields;
    handle->store = store;
    handle->entry_index = index;
    handle->entry_generation = entry->generation;
    handle->requested_fields = requested_fields;
    handle->next = store->handles;
    store->handles = handle;
    store->live_handles++;
    xSemaphoreGive(store->lock);
    *out_handle = handle;
    return ESP_OK;
}

void ha_entity_store_release(ha_entity_handle_t *handle)
{
    if (!handle || !handle->store) {
        return;
    }
    ha_entity_store_t *store = handle->store;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    ha_entity_handle_t **link = &store->handles;
    while (*link && *link != handle) {
        link = &(*link)->next;
    }
    if (*link == handle) {
        *link = handle->next;
        entity_entry_t *entry = &store->entries[handle->entry_index];
        if (entry->occupied && entry->generation == handle->entry_generation && entry->ref_count > 0) {
            entry->ref_count--;
            if (entry->ref_count > 0) {
                uint32_t fields = 0;
                for (ha_entity_handle_t *item = store->handles; item; item = item->next) {
                    if (item->entry_index == handle->entry_index &&
                        item->entry_generation == handle->entry_generation) {
                        fields |= item->requested_fields;
                    }
                }
                entry->requested_fields = fields;
            }
        }
        store->live_handles--;
    }
    xSemaphoreGive(store->lock);
    handle->store = NULL;
    free(handle);
}

void ha_entity_store_prune_unreferenced(ha_entity_store_t *store)
{
    if (!store) {
        return;
    }

    xSemaphoreTake(store->lock, portMAX_DELAY);
    for (size_t i = 0; i < store->max_entities; i++) {
        entity_entry_t *entry = &store->entries[i];
        if (entry->occupied && entry->ref_count == 0) {
            free_entry(entry);
            store->entity_count--;
        }
    }
    xSemaphoreGive(store->lock);
}

esp_err_t ha_entity_store_get(ha_entity_store_t *store,
                              const char *entity_id,
                              ha_entity_snapshot_t *out)
{
    if (!store || !entity_id || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    if (!entry) {
        xSemaphoreGive(store->lock);
        return ESP_ERR_NOT_FOUND;
    }
    copy_string(out->entity_id, sizeof(out->entity_id), entry->entity_id);
    copy_string(out->state, sizeof(out->state), entry->state);
    const char *friendly_name = entry->attributes ? json_string(entry->attributes, "friendly_name") : NULL;
    const char *unit = entry->attributes ? json_string(entry->attributes, "unit_of_measurement") : NULL;
    const char *device_class = entry->attributes ? json_string(entry->attributes, "device_class") : NULL;
    copy_string(out->friendly_name, sizeof(out->friendly_name), friendly_name);
    copy_string(out->unit_of_measurement, sizeof(out->unit_of_measurement), unit);
    copy_string(out->device_class, sizeof(out->device_class), device_class);
    copy_string(out->last_updated,
                sizeof(out->last_updated),
                entry->last_updated);
    copy_string(out->last_changed, sizeof(out->last_changed), entry->last_changed);
    out->revision = entry->revision;
    out->updated_at_ms = entry->updated_at_ms;
    out->sync_status = entry->sync_status;
    xSemaphoreGive(store->lock);
    return ESP_OK;
}

esp_err_t ha_entity_store_get_attr_string(ha_entity_store_t *store,
                                          const char *entity_id,
                                          const char *attribute,
                                          char *buffer,
                                          size_t buffer_size)
{
    if (!store || !entity_id || !attribute || !buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    const cJSON *item = entry && entry->attributes
                            ? cJSON_GetObjectItemCaseSensitive(entry->attributes, attribute)
                            : NULL;
    if (!cJSON_IsString(item)) {
        xSemaphoreGive(store->lock);
        return ESP_ERR_NOT_FOUND;
    }
    copy_string(buffer, buffer_size, item->valuestring);
    xSemaphoreGive(store->lock);
    return ESP_OK;
}

esp_err_t ha_entity_store_get_attr_number(ha_entity_store_t *store,
                                          const char *entity_id,
                                          const char *attribute,
                                          double *out)
{
    if (!store || !entity_id || !attribute || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    const cJSON *item = entry && entry->attributes
                            ? cJSON_GetObjectItemCaseSensitive(entry->attributes, attribute)
                            : NULL;
    if (!cJSON_IsNumber(item)) {
        xSemaphoreGive(store->lock);
        return ESP_ERR_NOT_FOUND;
    }
    *out = item->valuedouble;
    xSemaphoreGive(store->lock);
    return ESP_OK;
}

esp_err_t ha_entity_store_get_attr_bool(ha_entity_store_t *store,
                                        const char *entity_id,
                                        const char *attribute,
                                        bool *out)
{
    if (!store || !entity_id || !attribute || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    const cJSON *item = entry && entry->attributes
                            ? cJSON_GetObjectItemCaseSensitive(entry->attributes, attribute)
                            : NULL;
    if (!cJSON_IsBool(item)) {
        xSemaphoreGive(store->lock);
        return ESP_ERR_NOT_FOUND;
    }
    *out = cJSON_IsTrue(item);
    xSemaphoreGive(store->lock);
    return ESP_OK;
}

esp_err_t ha_entity_store_dup_attr_json(ha_entity_store_t *store,
                                        const char *entity_id,
                                        const char *attribute,
                                        cJSON **out)
{
    if (!store || !entity_id || !attribute || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    entity_entry_t *entry = find_entry_locked(store, entity_id, NULL);
    const cJSON *item = entry && entry->attributes
                            ? cJSON_GetObjectItemCaseSensitive(entry->attributes,
                                                               attribute)
                            : NULL;
    const bool found = item != NULL;
    cJSON *copy = item ? cJSON_Duplicate(item, true) : NULL;
    xSemaphoreGive(store->lock);
    if (!found) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    *out = copy;
    return ESP_OK;
}

ha_entity_store_status_t ha_entity_store_get_status(ha_entity_store_t *store)
{
    if (!store) {
        return HA_ENTITY_STORE_STOPPED;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    ha_entity_store_status_t status = store->status;
    xSemaphoreGive(store->lock);
    return status;
}

static esp_err_t add_listener(ha_entity_store_t *store,
                              listener_kind_t kind,
                              listener_callback_t callback,
                              void *user_ctx,
                              ha_entity_listener_handle_t **out_handle)
{
    bool has_callback = kind == LISTENER_STORE_EVENT ? callback.store_event != NULL
                                                     : callback.entity_changed != NULL;
    if (!store || !has_callback || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    ha_entity_listener_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(store->lock, portMAX_DELAY);
    size_t index = 0;
    for (; index < MAX_LISTENERS; index++) {
        if (!store->listeners[index].active) {
            break;
        }
    }
    if (index == MAX_LISTENERS) {
        xSemaphoreGive(store->lock);
        free(handle);
        return ESP_ERR_NO_MEM;
    }
    listener_slot_t *slot = &store->listeners[index];
    slot->active = true;
    slot->kind = kind;
    slot->generation = ++store->next_listener_generation;
    slot->user_ctx = user_ctx;
    if (kind == LISTENER_STORE_EVENT) {
        slot->callback.store_event = callback.store_event;
    } else {
        slot->callback.entity_changed = callback.entity_changed;
    }
    handle->store = store;
    handle->slot_index = index;
    handle->slot_generation = slot->generation;
    store->live_listener_handles++;
    xSemaphoreGive(store->lock);
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t ha_entity_store_add_event_listener(ha_entity_store_t *store,
                                             ha_entity_store_event_cb_t callback,
                                             void *user_ctx,
                                             ha_entity_listener_handle_t **out_handle)
{
    listener_callback_t listener_callback = {.store_event = callback};
    return add_listener(store, LISTENER_STORE_EVENT, listener_callback, user_ctx, out_handle);
}

esp_err_t ha_entity_store_add_listener(ha_entity_store_t *store,
                                       ha_entity_store_listener_t callback,
                                       void *user_ctx,
                                       ha_entity_listener_handle_t **out_handle)
{
    listener_callback_t listener_callback = {.entity_changed = callback};
    return add_listener(store, LISTENER_ENTITY_CHANGED, listener_callback, user_ctx, out_handle);
}

void ha_entity_store_remove_listener(ha_entity_listener_handle_t *handle)
{
    if (!handle || !handle->store) {
        return;
    }
    ha_entity_store_t *store = handle->store;
    xSemaphoreTake(store->lock, portMAX_DELAY);
    listener_slot_t *slot = &store->listeners[handle->slot_index];
    if (slot->active && slot->generation == handle->slot_generation) {
        memset(slot, 0, sizeof(*slot));
        store->live_listener_handles--;
    }
    xSemaphoreGive(store->lock);
    handle->store = NULL;
    free(handle);
}
