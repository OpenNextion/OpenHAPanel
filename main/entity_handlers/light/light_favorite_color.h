#ifndef OPEN_NEXTION_LIGHT_FAVORITE_COLOR_H
#define OPEN_NEXTION_LIGHT_FAVORITE_COLOR_H

#include <stdint.h>

#include "components/light_component_types.h"

#define LIGHT_FAVORITE_COLOR_MAX 8

typedef enum {
    LIGHT_FAVORITE_COLOR_TEMP = 0,
    LIGHT_FAVORITE_RGB,
    LIGHT_FAVORITE_HS,
    LIGHT_FAVORITE_RGBW,
    LIGHT_FAVORITE_RGBWW,
} light_favorite_color_type_t;

typedef struct {
    double hue;
    double saturation;
} light_hs_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t white;
} light_rgbw_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t cold_white;
    uint8_t warm_white;
} light_rgbww_t;

typedef struct {
    light_favorite_color_type_t type;
    light_rgb_t display_rgb;
    union {
        uint16_t color_temp_kelvin;
        light_rgb_t rgb;
        light_hs_t hs;
        light_rgbw_t rgbw;
        light_rgbww_t rgbww;
    } value;
} light_favorite_color_t;

#endif
