#ifndef OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_TILE_MODEL_H
#define OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_TILE_MODEL_H

#include <stdbool.h>
#include <time.h>

#include "ha_entity_store_type.h"
#include "lvgl.h"

#define ENTITY_DISPLAY_STATE_TEXT_MAX (HA_ENTITY_STORE_STATE_MAX + 24)

typedef enum {
    ENTITY_STATE_TEXT_STATIC = 0,
    ENTITY_STATE_TEXT_RELATIVE_TIME,
} entity_state_text_type_t;

/** Presentation model consumed by ha_tile_card. */
typedef struct {
    char title[HA_ENTITY_STORE_FRIENDLY_NAME_MAX];
    char state_text[ENTITY_DISPLAY_STATE_TEXT_MAX];
    entity_state_text_type_t state_text_type;
    time_t state_time;
    const char *icon;
    const char *state_icon;
    bool show_state_icon;
    bool unavailable;
    bool has_ripple_color;
    lv_color_t background_color;
    lv_color_t icon_color;
    lv_color_t icon_circle_background_color;
    lv_color_t ripple_color;
    lv_color_t title_color;
    lv_color_t state_color;
} entity_display_model_t;

#endif // OPEN_NEXTION_MAIN_ENTITY_HANDLERS_ENTITY_TILE_MODEL_H
