#include "screen_theme.h"

/* The dark values preserve the existing screen appearance. Values not yet
 * consumed by a migrated view establish the shared semantic vocabulary for
 * subsequent page migrations. */
const screen_theme_palette_t screen_theme_dark_palette = {
    .page_bg = 0x010101,
    .surface = 0x1E2228,
    .surface_elevated = 0x272B32,
    .surface_pressed = 0x343A42,

    .text_primary = 0xFFFFFF,
    .text_secondary = 0xFFFFFF,
    .text_disabled = 0x6E737C,
    .text_on_overlay = 0xFFFFFF,

    .divider = 0x34383F,
    .border = 0x34383F,

    .icon_primary = 0xFFFFFF,
    .icon_secondary = 0xA7ABB2,

    .control_track = 0x2A2D33,
    .control_indicator = 0x65558F,
    .control_knob = 0xFFFFFF,

    .overlay = 0x000000,
};

const screen_theme_palette_t screen_theme_light_palette = {
    .page_bg = 0xF4F5F7,
    .surface = 0xFFFFFF,
    .surface_elevated = 0xECEFF3,
    .surface_pressed = 0xE2E6EC,

    .text_primary = 0x1A1D21,
    .text_secondary = 0x667085,
    .text_disabled = 0x9AA1AC,
    .text_on_overlay = 0xFFFFFF,

    .divider = 0xD9DEE7,
    .border = 0xD9DEE7,

    .icon_primary = 0x1A1D21,
    .icon_secondary = 0x667085,

    .control_track = 0xD9DEE7,
    .control_indicator = 0x65558F,
    .control_knob = 0xFFFFFF,

    .overlay = 0x000000,
};

const screen_theme_palette_t *screen_theme_get_palette_for_mode(
    screen_theme_mode_t mode)
{
    switch (mode) {
        case SCREEN_THEME_LIGHT:
            return &screen_theme_light_palette;
        case SCREEN_THEME_DARK:
        default:
            return &screen_theme_dark_palette;
    }
}

const screen_theme_palette_t *screen_theme_get_palette(void)
{
    return screen_theme_get_palette_for_mode(screen_theme_get());
}
