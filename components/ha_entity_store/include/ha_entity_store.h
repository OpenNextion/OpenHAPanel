#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "esp_err.h"
#include "ha_entity_store_type.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ha_entity_store_create(const ha_entity_store_config_t *config,
                                 ha_entity_store_t **out_store);
esp_err_t ha_entity_store_start(ha_entity_store_t *store);
esp_err_t ha_entity_store_stop(ha_entity_store_t *store);
esp_err_t ha_entity_store_destroy(ha_entity_store_t *store);

/** Rebuild the filtered subscribe_entities subscription and refresh its snapshot. */
esp_err_t ha_entity_store_sync(ha_entity_store_t *store);

esp_err_t ha_entity_store_acquire(ha_entity_store_t *store,
                                  const char *entity_id,
                                  uint32_t requested_fields,
                                  ha_entity_handle_t **out_handle);
void ha_entity_store_release(ha_entity_handle_t *handle);

/**
 * Remove cached entities that are no longer referenced by any consumer.
 *
 * Releasing a handle preserves its snapshot so a screen configuration reload
 * can immediately reuse data for entities that remain configured. Call this
 * after all consumers have rebound their handles to discard entities removed
 * from the new configuration.
 */
void ha_entity_store_prune_unreferenced(ha_entity_store_t *store);

esp_err_t ha_entity_store_get(ha_entity_store_t *store,
                              const char *entity_id,
                              ha_entity_snapshot_t *out);
esp_err_t ha_entity_store_get_attr_string(ha_entity_store_t *store,
                                          const char *entity_id,
                                          const char *attribute,
                                          char *buffer,
                                          size_t buffer_size);
esp_err_t ha_entity_store_get_attr_number(ha_entity_store_t *store,
                                          const char *entity_id,
                                          const char *attribute,
                                          double *out);
esp_err_t ha_entity_store_get_attr_bool(ha_entity_store_t *store,
                                        const char *entity_id,
                                        const char *attribute,
                                        bool *out);
/** Duplicate one attribute while the store lock is held. Caller owns *out. */
esp_err_t ha_entity_store_dup_attr_json(ha_entity_store_t *store,
                                        const char *entity_id,
                                        const char *attribute,
                                        cJSON **out);

ha_entity_store_status_t ha_entity_store_get_status(ha_entity_store_t *store);

esp_err_t ha_entity_store_add_event_listener(ha_entity_store_t *store,
                                             ha_entity_store_event_cb_t callback,
                                             void *user_ctx,
                                             ha_entity_listener_handle_t **out_handle);
esp_err_t ha_entity_store_add_listener(ha_entity_store_t *store,
                                       ha_entity_store_listener_t callback,
                                       void *user_ctx,
                                       ha_entity_listener_handle_t **out_handle);
void ha_entity_store_remove_listener(ha_entity_listener_handle_t *handle);

#ifdef __cplusplus
}
#endif
