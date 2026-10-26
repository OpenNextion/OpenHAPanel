#ifndef OPEN_NEXTION_COMPONENTS_SCREEN_ROTATION_H
#define OPEN_NEXTION_COMPONENTS_SCREEN_ROTATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_lcd.h"
#include "esp_err.h"

typedef esp_err_t (*screen_rotation_transition_prepare_cb_t)(void *user_data);
typedef void (*screen_rotation_transition_complete_cb_t)(esp_err_t result,
                                                         void *user_data);

/**
 * Restore the persisted rotation and prepare the rotation configuration
 * service. Call this after bsp_lcd_init() and before creating any screen UI.
 */
esp_err_t screen_rotation_init(void);

/**
 * Register UI lifecycle hooks for runtime rotation changes.
 *
 * prepare_cb runs synchronously before the display rotation changes. The
 * rotation is aborted if it returns an error. complete_cb always runs after a
 * successful prepare, including when applying or persisting the rotation
 * fails, so the caller can rebuild UI that was hidden by prepare_cb.
 */
void screen_rotation_set_transition_callbacks(
    screen_rotation_transition_prepare_cb_t prepare_cb,
    screen_rotation_transition_complete_cb_t complete_cb,
    void *user_data);

/**
 * Apply and persist an absolute display rotation.
 *
 * The first implementation phase accepts only 0 and 90 degrees. Reapplying
 * the current rotation succeeds without touching the display or NVS.
 */
esp_err_t screen_rotation_set(bsp_lcd_rotation_t rotation);

/** Return the rotation currently applied by the LCD BSP. */
bsp_lcd_rotation_t screen_rotation_get(void);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_SCREEN_ROTATION_H
