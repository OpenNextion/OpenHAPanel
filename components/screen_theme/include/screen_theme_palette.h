#ifndef OPEN_NEXTION_COMPONENTS_SCREEN_THEME_PALETTE_H
#define OPEN_NEXTION_COMPONENTS_SCREEN_THEME_PALETTE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint32_t page_bg;
    uint32_t surface;
    uint32_t surface_elevated;
    uint32_t surface_pressed;

    uint32_t text_primary;
    uint32_t text_secondary;
    uint32_t text_disabled;
    uint32_t text_on_overlay;

    uint32_t divider;
    uint32_t border;

    uint32_t icon_primary;
    uint32_t icon_secondary;

    uint32_t control_track;
    uint32_t control_indicator;
    uint32_t control_knob;

    uint32_t overlay;
} screen_theme_palette_t;

extern const screen_theme_palette_t screen_theme_dark_palette;
extern const screen_theme_palette_t screen_theme_light_palette;

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_SCREEN_THEME_PALETTE_H
