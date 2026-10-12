#ifndef OPEN_NEXTION_MAIN_SCREEN_MORE_INFO_SCREEN_H
#define OPEN_NEXTION_MAIN_SCREEN_MORE_INFO_SCREEN_H

#include <stdbool.h>

#include "ha_entity_store.h"
#include "screen_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Load a clean More Info screen while retaining the current screen. */
bool more_info_screen_open(ha_entity_store_t *store,
                           const char *entity_id,
                           const screen_tile_card_data_t *tile_config);
/** Animate back to the retained screen. The TileView is not recreated. */
void more_info_screen_close(void);
/** Refresh the visible More Info screen when its entity changes. */
bool more_info_screen_refresh_entity(ha_entity_store_t *store,
                                     const char *entity_id);
bool more_info_screen_is_open(void);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_MORE_INFO_SCREEN_H
