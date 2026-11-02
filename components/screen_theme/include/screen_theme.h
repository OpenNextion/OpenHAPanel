#ifndef OPEN_NEXTION_COMPONENTS_SCREEN_THEME_H
#define OPEN_NEXTION_COMPONENTS_SCREEN_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "esp_err.h"
#include "esp_event.h"
#include "screen_theme_palette.h"

ESP_EVENT_DECLARE_BASE(SCREEN_THEME_EVENT);

typedef enum {
    SCREEN_THEME_DARK = 0,
    SCREEN_THEME_LIGHT,
} screen_theme_mode_t;

typedef enum {
    SCREEN_EVENT_THEME_CHANGED,
} screen_theme_event_t;

typedef struct {
    screen_theme_mode_t old_theme;
    screen_theme_mode_t new_theme;
} screen_theme_changed_event_t;

/**
 * Restore the persisted screen theme and initialize the configuration service.
 *
 * Missing or invalid persisted values select the safe default dark theme.
 * Call this before creating the screen UI so the first frame can use the
 * persisted theme.
 */
esp_err_t screen_theme_init(void);

/**
 * Persist and select an absolute screen theme.
 *
 * Reapplying the current theme succeeds without writing NVS. This service
 * does not manipulate LVGL objects; UI application is handled separately.
 */
esp_err_t screen_theme_set(screen_theme_mode_t mode);

/**
 * Persist and select a theme, reporting whether the stored value changed.
 * changed may be NULL when the caller does not need this information.
 */
esp_err_t screen_theme_set_with_changed(screen_theme_mode_t mode,
                                        bool *changed);

/** Return the selected theme, or dark before initialization. */
screen_theme_mode_t screen_theme_get(void);

/** Return the immutable palette for the currently selected theme. */
const screen_theme_palette_t *screen_theme_get_palette(void);

/** Return the immutable palette for a theme, or the dark palette if invalid. */
const screen_theme_palette_t *screen_theme_get_palette_for_mode(
    screen_theme_mode_t mode);

/** Return true when mode is one of the supported screen themes. */
bool screen_theme_is_valid(screen_theme_mode_t mode);

/** Return the stable protocol/configuration string for a valid theme. */
const char *screen_theme_mode_to_string(screen_theme_mode_t mode);

/** Parse an exact, case-sensitive "dark" or "light" theme string. */
esp_err_t screen_theme_mode_from_string(const char *value,
                                        screen_theme_mode_t *mode);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_SCREEN_THEME_H
