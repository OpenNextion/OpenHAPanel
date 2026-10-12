#ifndef OPEN_NEXTION_MAIN_SCREEN_MAIN_VIEW_H
#define OPEN_NEXTION_MAIN_SCREEN_MAIN_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "screen_config.h"
#include "ha_entity_store_type.h"

typedef struct main_view main_view_t;

main_view_t *main_view_create(lv_obj_t *parent, const screen_view_t *view);
void main_view_destroy(main_view_t *main_view);

lv_obj_t *main_view_get_obj(main_view_t *main_view);
void main_view_update(main_view_t *main_view, const screen_view_t *view);

/**
 * Update one badge by its position in ScreenView.badges.
 * A NULL name or state keeps the current value unchanged.
 */
bool main_view_update_badge(main_view_t *main_view,
                            size_t badge_index,
                            const char *name,
                            const char *state);

/**
 * Update every badge/tile bound to entity_id in this page.
 * Returns the number of updated UI items.
 */
size_t main_view_update_entity(main_view_t *main_view,
                               const char *entity_id,
                               const char *name,
                               const char *state);

/** Refresh every tile bound to entity_id using the Entity Store cache. */
size_t main_view_refresh_entity(main_view_t *main_view,
                                ha_entity_store_t *store,
                                const char *entity_id);

/** Release old subscriptions and bind all tile cards to a new Entity Store. */
size_t main_view_set_entity_store(main_view_t *main_view,
                                  ha_entity_store_t *store);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_MAIN_VIEW_H
