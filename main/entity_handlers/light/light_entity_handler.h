#ifndef OPEN_NEXTION_MAIN_ENTITY_HANDLERS_LIGHT_ENTITY_HANDLER_H
#define OPEN_NEXTION_MAIN_ENTITY_HANDLERS_LIGHT_ENTITY_HANDLER_H

#include "../entity_handler.h"
#include "light_favorite_color.h"

extern const entity_handler_t light_entity_handler;
/** Shared tile/feature color derived from the current light color mode. */
lv_color_t light_entity_active_color(ha_entity_store_t *store,
                                     const char *entity_id);
/** Toggle accent color: rgb/color-temp color, or amber for plain lights. */
lv_color_t light_entity_toggle_color(ha_entity_store_t *store,
                                     const char *entity_id);
/** True when the light currently exposes an rgb_color attribute. */
bool light_entity_has_rgb_color(ha_entity_store_t *store,
                                const char *entity_id);
size_t light_entity_favorite_palette(ha_entity_store_t *store,
                                     const char *entity_id,
                                     light_favorite_color_t colors[LIGHT_FAVORITE_COLOR_MAX]);
void light_send_preset_color(const char *entity_id,
                             const light_favorite_color_t *color);

#endif // OPEN_NEXTION_MAIN_ENTITY_HANDLERS_LIGHT_ENTITY_HANDLER_H
