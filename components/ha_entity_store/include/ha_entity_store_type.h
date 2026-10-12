#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "ha_ws_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HA_ENTITY_STORE_ENTITY_ID_MAX 128
#define HA_ENTITY_STORE_STATE_MAX 64
#define HA_ENTITY_STORE_FRIENDLY_NAME_MAX 128
#define HA_ENTITY_STORE_UNIT_MAX 32
#define HA_ENTITY_STORE_DEVICE_CLASS_MAX 48
/* Room for an ISO 8601 timestamp such as 2026-09-20T13:24:31.123456+00:00. */
#define HA_ENTITY_STORE_DATETIME_MAX 40

#define HA_ENTITY_FIELD_STATE (1U << 0)
#define HA_ENTITY_FIELD_ATTRIBUTES (1U << 1)
#define HA_ENTITY_FIELD_ALL (HA_ENTITY_FIELD_STATE | HA_ENTITY_FIELD_ATTRIBUTES)

/* Public opaque object types. Their layouts remain private to the component. */
typedef struct ha_entity_store ha_entity_store_t;
typedef struct ha_entity_handle ha_entity_handle_t;
typedef struct ha_entity_listener_handle ha_entity_listener_handle_t;

typedef enum {
    HA_ENTITY_STORE_STOPPED = 0,
    HA_ENTITY_STORE_OFFLINE,
    HA_ENTITY_STORE_SUBSCRIBING,
    HA_ENTITY_STORE_SYNCING,
    HA_ENTITY_STORE_READY,
    HA_ENTITY_STORE_DEGRADED,
} ha_entity_store_status_t;

typedef enum {
    HA_ENTITY_SYNC_UNKNOWN = 0,
    HA_ENTITY_SYNC_SYNCING,
    HA_ENTITY_SYNC_READY,
    HA_ENTITY_SYNC_STALE,
    HA_ENTITY_SYNC_REMOVED,
} ha_entity_sync_status_t;

typedef enum {
    HA_ENTITY_CHANGE_STATE = 1U << 0,
    HA_ENTITY_CHANGE_ATTRIBUTES = 1U << 1,
    HA_ENTITY_CHANGE_AVAILABILITY = 1U << 2,
    HA_ENTITY_CHANGE_ADDED = 1U << 3,
    HA_ENTITY_CHANGE_REMOVED = 1U << 4,
} ha_entity_change_mask_t;

typedef enum {
    HA_ENTITY_STORE_EVENT_STATUS_CHANGED = 0,
    HA_ENTITY_STORE_EVENT_SYNC_STARTED,
    HA_ENTITY_STORE_EVENT_SYNC_SUCCEEDED,
    HA_ENTITY_STORE_EVENT_SYNC_FAILED,
} ha_entity_store_event_type_t;

typedef struct {
    ha_entity_store_event_type_t type;
    esp_err_t result;
    uint32_t sync_generation;
    size_t matched_entities;
    size_t changed_entities;
    ha_entity_store_status_t status;
} ha_entity_store_event_t;

typedef struct {
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    char state[HA_ENTITY_STORE_STATE_MAX];
    char friendly_name[HA_ENTITY_STORE_FRIENDLY_NAME_MAX];
    char unit_of_measurement[HA_ENTITY_STORE_UNIT_MAX];
    char device_class[HA_ENTITY_STORE_DEVICE_CLASS_MAX];
    /* Home Assistant last_updated: when the entity was last updated. */
    char last_updated[HA_ENTITY_STORE_DATETIME_MAX];
    /* Home Assistant last_changed: when the state itself last changed. */
    char last_changed[HA_ENTITY_STORE_DATETIME_MAX];
    uint32_t revision;
    uint32_t updated_at_ms;
    ha_entity_sync_status_t sync_status;
} ha_entity_snapshot_t;

typedef struct {
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    uint32_t revision;
    uint32_t changes;
} ha_entity_changed_event_t;

typedef void (*ha_entity_store_event_cb_t)(ha_entity_store_t *store,
                                           const ha_entity_store_event_t *event,
                                           void *user_ctx);

typedef void (*ha_entity_store_listener_t)(ha_entity_store_t *store,
                                           const ha_entity_changed_event_t *event,
                                           void *user_ctx);

typedef struct {
    ha_ws_client_t *ws_client;
    size_t max_entities;
    size_t event_queue_size;
    uint32_t worker_stack_size;
    int worker_priority;
    /** Prefer PSRAM for the worker stack and entity table, with internal-RAM fallback. */
    bool prefer_external_memory;
} ha_entity_store_config_t;

#ifdef __cplusplus
}
#endif
