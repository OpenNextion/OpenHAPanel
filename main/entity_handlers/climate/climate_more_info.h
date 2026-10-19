#ifndef OPEN_NEXTION_CLIMATE_MORE_INFO_H
#define OPEN_NEXTION_CLIMATE_MORE_INFO_H

#include "entity_handlers/entity_handler.h"

esp_err_t climate_more_info_render(const entity_more_info_context_t *context);
esp_err_t climate_more_info_update(const entity_more_info_context_t *context);

#endif
