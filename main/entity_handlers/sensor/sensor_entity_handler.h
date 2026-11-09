#ifndef OPEN_NEXTION_MAIN_ENTITY_HANDLERS_SENSOR_ENTITY_HANDLER_H
#define OPEN_NEXTION_MAIN_ENTITY_HANDLERS_SENSOR_ENTITY_HANDLER_H

#include "../entity_handler.h"

extern const entity_handler_t sensor_entity_handler;

/** Match the More Info history mode: true for chart, false for text history. */
bool sensor_entity_has_chart_history(const ha_entity_snapshot_t *snapshot);

/** Color shared by the Sensor Tile icon and its trend graph Feature. */
uint32_t sensor_entity_icon_color(const ha_entity_snapshot_t *snapshot);

#endif // OPEN_NEXTION_MAIN_ENTITY_HANDLERS_SENSOR_ENTITY_HANDLER_H
