#ifndef OPEN_NEXTION_COMPONENTS_UI_PREFERENCES_H
#define OPEN_NEXTION_COMPONENTS_UI_PREFERENCES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/** Read whether the HA provisioning swipe hint has been acknowledged. */
esp_err_t ui_preferences_get_ha_swipe_hint_seen(bool *seen);

/** Persist that the HA provisioning swipe hint has been acknowledged. */
esp_err_t ui_preferences_set_ha_swipe_hint_seen(void);

/** Read the persisted display rotation as a number of clockwise quarter turns. */
esp_err_t ui_preferences_get_screen_rotation(uint8_t *quarter_turns);

/** Persist the display rotation as a number of clockwise quarter turns. */
esp_err_t ui_preferences_set_screen_rotation(uint8_t quarter_turns);

/** Read the persisted screen theme value. */
esp_err_t ui_preferences_get_screen_theme(uint8_t *theme);

/** Persist the screen theme value. */
esp_err_t ui_preferences_set_screen_theme(uint8_t theme);

/** Clear all persisted UI preferences during a factory reset. */
esp_err_t ui_preferences_prepare_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_UI_PREFERENCES_H
