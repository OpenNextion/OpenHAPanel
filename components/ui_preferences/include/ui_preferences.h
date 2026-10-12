#ifndef OPEN_NEXTION_COMPONENTS_UI_PREFERENCES_H
#define OPEN_NEXTION_COMPONENTS_UI_PREFERENCES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "esp_err.h"

/** Read whether the HA provisioning swipe hint has been acknowledged. */
esp_err_t ui_preferences_get_ha_swipe_hint_seen(bool *seen);

/** Persist that the HA provisioning swipe hint has been acknowledged. */
esp_err_t ui_preferences_set_ha_swipe_hint_seen(void);

/** Clear all persisted UI preferences during a factory reset. */
esp_err_t ui_preferences_prepare_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_UI_PREFERENCES_H
