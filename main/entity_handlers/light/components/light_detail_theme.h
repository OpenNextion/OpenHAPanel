#ifndef OPEN_NEXTION_LIGHT_DETAIL_THEME_H
#define OPEN_NEXTION_LIGHT_DETAIL_THEME_H

#include <stdbool.h>
#include <stdint.h>

#include "screen_theme.h"

/* Keep the embedded Light More Info palette aligned with Home Assistant's
 * semantic light-theme treatment: a neutral page, dark primary content,
 * softer secondary content, and translucent neutral control surfaces. */
#define LIGHT_DETAIL_LIGHT_BG             0xF4F5F7
#define LIGHT_DETAIL_LIGHT_TEXT_PRIMARY   0x333333
#define LIGHT_DETAIL_LIGHT_TEXT_SECONDARY 0x666666
#define LIGHT_DETAIL_LIGHT_SURFACE        0xDDE1E7
#define LIGHT_DETAIL_LIGHT_SURFACE_PRESS  0xC8CDD5
#define LIGHT_DETAIL_LIGHT_NEUTRAL        0x8B9197
#define LIGHT_DETAIL_LIGHT_DISABLED       0xBDBDBD
#define LIGHT_DETAIL_LIGHT_INACTIVE       0x9E9E9E
#define LIGHT_DETAIL_LIGHT_DIVIDER        0xD9DEE7

static inline bool light_detail_is_light_theme(void)
{
    return screen_theme_get() == SCREEN_THEME_LIGHT;
}

static inline uint32_t light_detail_theme_color(uint32_t dark_color,
                                                uint32_t light_color)
{
    return light_detail_is_light_theme() ? light_color : dark_color;
}

#endif // OPEN_NEXTION_LIGHT_DETAIL_THEME_H
