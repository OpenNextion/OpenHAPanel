#ifndef OPEN_NEXTION_CLIMATE_DETAIL_THEME_H
#define OPEN_NEXTION_CLIMATE_DETAIL_THEME_H

#include <stdbool.h>
#include <stdint.h>

#include "screen_theme.h"

/* Embedded equivalents of Home Assistant's semantic Climate More Info
 * colors. HVAC mode colors remain state-driven and are intentionally not
 * included here. */
#define CLIMATE_DETAIL_LIGHT_BG             0xF4F5F7
#define CLIMATE_DETAIL_LIGHT_TEXT_PRIMARY   0x333333
#define CLIMATE_DETAIL_LIGHT_TEXT_SECONDARY 0x666666
#define CLIMATE_DETAIL_LIGHT_CONTROL_BG      0xe2e3e4
#define CLIMATE_DETAIL_LIGHT_SURFACE        0xDDE1E7
#define CLIMATE_DETAIL_LIGHT_SURFACE_PRESS  0xC8CDD5
#define CLIMATE_DETAIL_LIGHT_NEUTRAL        0x8B9197
#define CLIMATE_DETAIL_LIGHT_DISABLED       0xBDBDBD
#define CLIMATE_DETAIL_LIGHT_INACTIVE       0x9E9E9E
#define CLIMATE_DETAIL_LIGHT_DIVIDER        0xD9DEE7

static inline bool climate_detail_is_light_theme(void)
{
    return screen_theme_get() == SCREEN_THEME_LIGHT;
}

static inline uint32_t climate_detail_theme_color(uint32_t dark_color,
                                                  uint32_t light_color)
{
    return climate_detail_is_light_theme() ? light_color : dark_color;
}

#endif // OPEN_NEXTION_CLIMATE_DETAIL_THEME_H
