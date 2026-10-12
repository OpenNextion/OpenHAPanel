#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_VALIDATE_H
#define OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_VALIDATE_H

#include "cJSON.h"
#include "esp_err.h"

/**
 * @brief Validate a screen configuration JSON object against the screen page schema.
 *
 * Checks required objects/arrays, supported enum values, card and badge data
 * shape, action values, feature types, and full_width boolean typing.
 *
 * @param config cJSON object containing the screen config data.
 * @return ESP_OK if valid, ESP_FAIL (-1) if invalid.
 */
esp_err_t screen_validate_config(const cJSON *config);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_VALIDATE_H
