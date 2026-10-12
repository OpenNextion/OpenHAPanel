#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_MAIN_SCREEN_SCREEN_CONFIG_H
#define OPEN_NEXTION_MAIN_SCREEN_SCREEN_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "esp_err.h"

typedef enum {
    SCREEN_VIEW_TYPE_SECTIONS,
} screen_view_type_t;

typedef enum {
    SCREEN_BADGE_TYPE_ENTITY,
} screen_badge_type_t;

typedef enum {
    SCREEN_CARD_TYPE_HEADING,
    SCREEN_CARD_TYPE_TILE,
} screen_card_type_t;

typedef enum {
    SCREEN_ACTION_NOTHING,
    SCREEN_ACTION_TOGGLE,
    SCREEN_ACTION_MORE_INFO,
} screen_action_type_t;

typedef enum {
    SCREEN_FEATURE_LIGHT_BRIGHTNESS,
    SCREEN_FEATURE_LIGHT_COLOR_TEMP,
    SCREEN_FEATURE_COVER_OPEN_CLOSE,
    SCREEN_FEATURE_COVER_POSITION,
} screen_feature_type_t;

typedef struct {
    screen_action_type_t action;
} screen_action_t;

typedef struct {
    bool has_tap_action;
    bool has_hold_action;
    bool has_icon_tap_action;
    screen_action_t tap_action;
    screen_action_t hold_action;
    screen_action_t icon_tap_action;
} screen_action_config_t;

typedef struct {
    char *entity;
    char *name;
    bool has_action_config;
    screen_action_config_t action_config;
} screen_entity_badge_data_t;

typedef struct {
    screen_badge_type_t type;
    bool has_data;
    screen_entity_badge_data_t data;
} screen_badge_config_t;

typedef struct {
    char *heading;
} screen_heading_card_data_t;

typedef struct {
    screen_feature_type_t type;
    cJSON *data;
} screen_feature_config_t;

typedef struct {
    char *entity;
    char *name;
    bool full_width;
    bool has_action_config;
    screen_action_config_t action_config;
    size_t feature_count;
    screen_feature_config_t *features;
} screen_tile_card_data_t;

typedef struct {
    screen_card_type_t type;
    union {
        screen_heading_card_data_t heading;
        screen_tile_card_data_t tile;
    } data;
} screen_card_config_t;

typedef struct {
    int32_t index;
    char *title;
    char *icon;
    screen_view_type_t type;
    size_t badge_count;
    screen_badge_config_t *badges;
    size_t card_count;
    screen_card_config_t *cards;
} screen_view_t;

typedef struct {
    size_t screen_count;
    screen_view_t *screens;
} screen_config_t;

esp_err_t screen_parse_config(const cJSON *json, screen_config_t *out_config);
cJSON *screen_config_to_json(const screen_config_t *config);
void screen_free_config(screen_config_t *config);

#endif // OPEN_NEXTION_MAIN_SCREEN_SCREEN_CONFIG_H

#ifdef __cplusplus
}
#endif
