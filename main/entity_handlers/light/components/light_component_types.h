#ifndef OPEN_NEXTION_LIGHT_COMPONENT_TYPES_H
#define OPEN_NEXTION_LIGHT_COMPONENT_TYPES_H

#include <stdint.h>

typedef enum {
    LIGHT_CONTROL_PREVIEW = 0,
    LIGHT_CONTROL_COMMIT,
    LIGHT_CONTROL_CANCEL,
} light_control_phase_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} light_rgb_t;

typedef enum {
    LIGHT_MODE_BRIGHTNESS = 0,
    LIGHT_MODE_RGB,
    LIGHT_MODE_COLOR_TEMP,
} light_mode_t;

#define LIGHT_MODE_MASK_BRIGHTNESS (1U << LIGHT_MODE_BRIGHTNESS)
#define LIGHT_MODE_MASK_RGB        (1U << LIGHT_MODE_RGB)
#define LIGHT_MODE_MASK_COLOR_TEMP (1U << LIGHT_MODE_COLOR_TEMP)

#endif
