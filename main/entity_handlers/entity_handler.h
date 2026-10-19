#ifndef OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_HANDLER_H
#define OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_HANDLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "ha_entity_store.h"
#include "entity_tile_model.h"
#include "screen_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENTITY_HANDLER_REFRESH_CHANGES                                      \
    (HA_ENTITY_CHANGE_STATE | HA_ENTITY_CHANGE_ATTRIBUTES |                 \
     HA_ENTITY_CHANGE_AVAILABILITY | HA_ENTITY_CHANGE_ADDED |               \
     HA_ENTITY_CHANGE_REMOVED)

typedef esp_err_t (*entity_tile_render_cb_t)(ha_entity_store_t *store,
                                              const ha_entity_snapshot_t *snapshot,
                                              entity_display_model_t *model);

typedef enum {
    ENTITY_ACTION_TRIGGER_TAP,
    ENTITY_ACTION_TRIGGER_HOLD,
    ENTITY_ACTION_TRIGGER_ICON_TAP,
} entity_action_trigger_t;

typedef void (*entity_open_more_info_cb_t)(const char *entity_id,
                                           void *user_data);

typedef struct {
    const char *entity_id;
    const screen_tile_card_data_t *tile_config;
    const screen_action_config_t *action_config;
    entity_action_trigger_t trigger;
    entity_open_more_info_cb_t open_more_info;
    void *user_data;
} entity_action_context_t;

typedef bool (*entity_action_cb_t)(const entity_action_context_t *context);

typedef void (*entity_more_info_close_cb_t)(void *user_data);

/**
 * Complete rendering context for a domain-owned More Info screen.
 *
 * The handler owns every visual child it creates on screen. overlay is a
 * transparent top-layer host for controls that must stay above global input
 * regions. Call close to load the retained TileView screen again.
 */
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *overlay;
    ha_entity_store_t *store;
    ha_entity_snapshot_t snapshot;
    const screen_tile_card_data_t *tile_config;
    entity_more_info_close_cb_t close;
    void *close_user_data;
} entity_more_info_context_t;

typedef esp_err_t (*entity_more_info_render_cb_t)(
    const entity_more_info_context_t *context);
/** Update an existing More Info page without recreating its LVGL objects. */
typedef esp_err_t (*entity_more_info_update_cb_t)(
    const entity_more_info_context_t *context);

/** Strategy implemented by one Home Assistant entity domain. */
typedef struct {
    const char *domain;
    uint32_t required_fields;
    uint32_t refresh_changes;
    const char *initial_icon;
    screen_action_config_t default_action_config;
    entity_action_cb_t handle_action;
    entity_tile_render_cb_t render_tile;
    entity_more_info_render_cb_t render_more_info;
    entity_more_info_update_cb_t update_more_info;
} entity_handler_t;

/** Shared helpers keep state normalization consistent across domain handlers. */
bool entity_handler_snapshot_is_unavailable(const ha_entity_snapshot_t *snapshot);
/**
 * Availability for stateless service entities such as button and scene.
 * Their initial "unknown" state means "never triggered", not unavailable.
 */
bool entity_handler_snapshot_is_service_unavailable(
    const ha_entity_snapshot_t *snapshot);
void entity_handler_apply_friendly_name(const ha_entity_snapshot_t *snapshot,
                                        entity_display_model_t *model);
void entity_handler_copy_snapshot_state(char *buffer,
                                        size_t buffer_size,
                                        const ha_entity_snapshot_t *snapshot);
void entity_handler_copy_state_text(char *buffer,
                                    size_t buffer_size,
                                    const char *state);
/** Resolve the configured action matching the event trigger. */
const screen_action_t *entity_handler_get_action(
    const entity_action_context_t *context);
/** Explicit helper for handlers that opt in to the More Info action. */
bool entity_handler_handle_more_info_action(
    const entity_action_context_t *context);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_HANDLER_H
