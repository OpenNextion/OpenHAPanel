#include "screen_config.h"

#include <stdlib.h>
#include <string.h>

#include "screen_validate.h"

static char *screen_strdup(const char *value)
{
    if (!value) {
        return NULL;
    }
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, value, len);
    }
    return copy;
}

static const char *action_type_to_string(screen_action_type_t action)
{
    switch (action) {
    case SCREEN_ACTION_TOGGLE:
        return "toggle";
    case SCREEN_ACTION_MORE_INFO:
        return "more-info";
    case SCREEN_ACTION_NOTHING:
    default:
        return "nothing";
    }
}

static const char *feature_type_to_string(screen_feature_type_t type)
{
    switch (type) {
    case SCREEN_FEATURE_LIGHT_COLOR_TEMP:
        return "light-color-temp";
    case SCREEN_FEATURE_COVER_OPEN_CLOSE:
        return "cover-open-close";
    case SCREEN_FEATURE_COVER_POSITION:
        return "cover-position";
    case SCREEN_FEATURE_LIGHT_BRIGHTNESS:
    default:
        return "light-brightness";
    }
}

static screen_action_type_t parse_action_type(const char *action)
{
    if (strcmp(action, "toggle") == 0) {
        return SCREEN_ACTION_TOGGLE;
    }
    if (strcmp(action, "more-info") == 0) {
        return SCREEN_ACTION_MORE_INFO;
    }
    return SCREEN_ACTION_NOTHING;
}

static screen_feature_type_t parse_feature_type(const char *type)
{
    if (strcmp(type, "light-color-temp") == 0) {
        return SCREEN_FEATURE_LIGHT_COLOR_TEMP;
    }
    if (strcmp(type, "cover-open-close") == 0) {
        return SCREEN_FEATURE_COVER_OPEN_CLOSE;
    }
    if (strcmp(type, "cover-position") == 0) {
        return SCREEN_FEATURE_COVER_POSITION;
    }
    return SCREEN_FEATURE_LIGHT_BRIGHTNESS;
}

static void parse_action_item(const cJSON *root, const char *key, bool *has_action, screen_action_t *out_action)
{
    const cJSON *item = root ? cJSON_GetObjectItemCaseSensitive(root, key) : NULL;
    const cJSON *action = item ? cJSON_GetObjectItemCaseSensitive(item, "action") : NULL;
    *has_action = cJSON_IsString(action) && action->valuestring;
    out_action->action = *has_action ? parse_action_type(action->valuestring) : SCREEN_ACTION_NOTHING;
}

static void parse_action_config(const cJSON *json, screen_action_config_t *out_config)
{
    parse_action_item(json, "tap_action", &out_config->has_tap_action, &out_config->tap_action);
    parse_action_item(json, "hold_action", &out_config->has_hold_action, &out_config->hold_action);
    parse_action_item(json, "icon_tap_action", &out_config->has_icon_tap_action, &out_config->icon_tap_action);
}

static esp_err_t parse_badges(const cJSON *badges_json, screen_view_t *view)
{
    view->badge_count = cJSON_GetArraySize(badges_json);
    if (view->badge_count == 0) {
        return ESP_OK;
    }

    view->badges = calloc(view->badge_count, sizeof(screen_badge_config_t));
    if (!view->badges) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < view->badge_count; i++) {
        const cJSON *badge_json = cJSON_GetArrayItem(badges_json, i);
        const cJSON *data_json = cJSON_GetObjectItemCaseSensitive(badge_json, "data");
        screen_badge_config_t *badge = &view->badges[i];
        badge->type = SCREEN_BADGE_TYPE_ENTITY;
        badge->has_data = cJSON_IsObject(data_json);

        if (badge->has_data) {
            const cJSON *entity = cJSON_GetObjectItemCaseSensitive(data_json, "entity");
            const cJSON *name = cJSON_GetObjectItemCaseSensitive(data_json, "name");
            const cJSON *action_config = cJSON_GetObjectItemCaseSensitive(data_json, "actionConfig");
            badge->data.entity = screen_strdup(entity->valuestring);
            badge->data.name = cJSON_IsString(name) ? screen_strdup(name->valuestring) : NULL;
            badge->data.has_action_config = cJSON_IsObject(action_config);
            if (badge->data.has_action_config) {
                parse_action_config(action_config, &badge->data.action_config);
            }
            if (!badge->data.entity) {
                return ESP_ERR_NO_MEM;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t parse_features(const cJSON *features_json, screen_tile_card_data_t *tile)
{
    if (!cJSON_IsArray(features_json)) {
        return ESP_OK;
    }

    tile->feature_count = cJSON_GetArraySize(features_json);
    if (tile->feature_count == 0) {
        return ESP_OK;
    }

    tile->features = calloc(tile->feature_count, sizeof(screen_feature_config_t));
    if (!tile->features) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < tile->feature_count; i++) {
        const cJSON *feature_json = cJSON_GetArrayItem(features_json, i);
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(feature_json, "type");
        const cJSON *data = cJSON_GetObjectItemCaseSensitive(feature_json, "data");
        tile->features[i].type = parse_feature_type(type->valuestring);
        tile->features[i].data = cJSON_IsObject(data) ? cJSON_Duplicate(data, true) : NULL;
        if (cJSON_IsObject(data) && !tile->features[i].data) {
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

static esp_err_t parse_heading_card_data(const cJSON *data_json, screen_heading_card_data_t *heading)
{
    const cJSON *heading_json = cJSON_GetObjectItemCaseSensitive(data_json, "heading");
    heading->heading = screen_strdup(heading_json->valuestring);
    return heading->heading ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t parse_tile_card_data(const cJSON *data_json, screen_tile_card_data_t *tile)
{
    const cJSON *entity = cJSON_GetObjectItemCaseSensitive(data_json, "entity");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(data_json, "name");
    const cJSON *full_width = cJSON_GetObjectItemCaseSensitive(data_json, "full_width");
    const cJSON *action_config = cJSON_GetObjectItemCaseSensitive(data_json, "actionConfig");
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(data_json, "features");

    tile->entity = screen_strdup(entity->valuestring);
    tile->name = cJSON_IsString(name) ? screen_strdup(name->valuestring) : NULL;
    tile->full_width = cJSON_IsTrue(full_width);
    tile->has_action_config = cJSON_IsObject(action_config);
    if (tile->has_action_config) {
        parse_action_config(action_config, &tile->action_config);
    }
    if (!tile->entity) {
        return ESP_ERR_NO_MEM;
    }

    return parse_features(features, tile);
}

static esp_err_t parse_cards(const cJSON *cards_json, screen_view_t *view)
{
    view->card_count = cJSON_GetArraySize(cards_json);
    if (view->card_count == 0) {
        return ESP_OK;
    }

    view->cards = calloc(view->card_count, sizeof(screen_card_config_t));
    if (!view->cards) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < view->card_count; i++) {
        const cJSON *card_json = cJSON_GetArrayItem(cards_json, i);
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(card_json, "type");
        const cJSON *data = cJSON_GetObjectItemCaseSensitive(card_json, "data");
        screen_card_config_t *card = &view->cards[i];

        if (strcmp(type->valuestring, "heading") == 0) {
            card->type = SCREEN_CARD_TYPE_HEADING;
            if (parse_heading_card_data(data, &card->data.heading) != ESP_OK) {
                return ESP_ERR_NO_MEM;
            }
        } else {
            card->type = SCREEN_CARD_TYPE_TILE;
            if (parse_tile_card_data(data, &card->data.tile) != ESP_OK) {
                return ESP_ERR_NO_MEM;
            }
        }
    }

    return ESP_OK;
}

esp_err_t screen_parse_config(const cJSON *json, screen_config_t *out_config)
{
    if (!out_config || screen_validate_config(json) != ESP_OK) {
        return ESP_FAIL;
    }

    memset(out_config, 0, sizeof(*out_config));
    const cJSON *screens = cJSON_GetObjectItemCaseSensitive(json, "screens");
    out_config->screen_count = cJSON_GetArraySize(screens);
    if (out_config->screen_count == 0) {
        return ESP_OK;
    }

    out_config->screens = calloc(out_config->screen_count, sizeof(screen_view_t));
    if (!out_config->screens) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < out_config->screen_count; i++) {
        const cJSON *screen_json = cJSON_GetArrayItem(screens, i);
        const cJSON *index = cJSON_GetObjectItemCaseSensitive(screen_json, "index");
        const cJSON *title = cJSON_GetObjectItemCaseSensitive(screen_json, "title");
        const cJSON *icon = cJSON_GetObjectItemCaseSensitive(screen_json, "icon");
        const cJSON *badges = cJSON_GetObjectItemCaseSensitive(screen_json, "badges");
        const cJSON *cards = cJSON_GetObjectItemCaseSensitive(screen_json, "cards");
        screen_view_t *view = &out_config->screens[i];

        view->index = index->valueint;
        view->title = screen_strdup(title->valuestring);
        view->icon = cJSON_IsString(icon) ? screen_strdup(icon->valuestring) : NULL;
        view->type = SCREEN_VIEW_TYPE_SECTIONS;
        if (!view->title) {
            screen_free_config(out_config);
            return ESP_ERR_NO_MEM;
        }

        if (parse_badges(badges, view) != ESP_OK || parse_cards(cards, view) != ESP_OK) {
            screen_free_config(out_config);
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

static void add_optional_string(cJSON *root, const char *key, const char *value)
{
    if (value) {
        cJSON_AddStringToObject(root, key, value);
    }
}

static void add_action_item(cJSON *root, const char *key, const screen_action_t *action)
{
    cJSON *action_json = cJSON_CreateObject();
    if (!action_json) {
        return;
    }
    cJSON_AddStringToObject(action_json, "action", action_type_to_string(action->action));
    cJSON_AddItemToObject(root, key, action_json);
}

static void add_action_config(cJSON *root, const screen_action_config_t *config)
{
    cJSON *action_config = cJSON_CreateObject();
    if (!action_config) {
        return;
    }

    if (config->has_tap_action) {
        add_action_item(action_config, "tap_action", &config->tap_action);
    }
    if (config->has_hold_action) {
        add_action_item(action_config, "hold_action", &config->hold_action);
    }
    if (config->has_icon_tap_action) {
        add_action_item(action_config, "icon_tap_action", &config->icon_tap_action);
    }
    cJSON_AddItemToObject(root, "actionConfig", action_config);
}

static cJSON *badge_to_json(const screen_badge_config_t *badge)
{
    cJSON *badge_json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if (!badge_json || !data) {
        cJSON_Delete(badge_json);
        cJSON_Delete(data);
        return NULL;
    }

    cJSON_AddStringToObject(badge_json, "type", "entity");
    if (badge->has_data) {
        cJSON_AddStringToObject(data, "entity", badge->data.entity);
        add_optional_string(data, "name", badge->data.name);
        if (badge->data.has_action_config) {
            add_action_config(data, &badge->data.action_config);
        }
        cJSON_AddItemToObject(badge_json, "data", data);
    } else {
        cJSON_Delete(data);
    }

    return badge_json;
}

static cJSON *heading_card_to_json(const screen_heading_card_data_t *heading)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return NULL;
    }
    cJSON_AddStringToObject(data, "heading", heading->heading);
    return data;
}

static cJSON *tile_card_to_json(const screen_tile_card_data_t *tile)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return NULL;
    }

    cJSON_AddStringToObject(data, "entity", tile->entity);
    add_optional_string(data, "name", tile->name);
    cJSON_AddBoolToObject(data, "full_width", tile->full_width);
    if (tile->has_action_config) {
        add_action_config(data, &tile->action_config);
    }
    if (tile->feature_count > 0) {
        cJSON *features = cJSON_CreateArray();
        if (!features) {
            cJSON_Delete(data);
            return NULL;
        }
        for (size_t i = 0; i < tile->feature_count; i++) {
            cJSON *feature = cJSON_CreateObject();
            if (!feature) {
                cJSON_Delete(data);
                return NULL;
            }
            cJSON_AddStringToObject(feature, "type", feature_type_to_string(tile->features[i].type));
            if (tile->features[i].data) {
                cJSON *feature_data = cJSON_Duplicate(tile->features[i].data, true);
                if (!feature_data) {
                    cJSON_Delete(data);
                    return NULL;
                }
                cJSON_AddItemToObject(feature, "data", feature_data);
            }
            cJSON_AddItemToArray(features, feature);
        }
        cJSON_AddItemToObject(data, "features", features);
    }

    return data;
}

static cJSON *card_to_json(const screen_card_config_t *card)
{
    cJSON *card_json = cJSON_CreateObject();
    if (!card_json) {
        return NULL;
    }

    cJSON_AddStringToObject(card_json, "type", card->type == SCREEN_CARD_TYPE_HEADING ? "heading" : "tile");
    cJSON *data = card->type == SCREEN_CARD_TYPE_HEADING ? heading_card_to_json(&card->data.heading)
                                                         : tile_card_to_json(&card->data.tile);
    if (!data) {
        cJSON_Delete(card_json);
        return NULL;
    }
    cJSON_AddItemToObject(card_json, "data", data);
    return card_json;
}

cJSON *screen_config_to_json(const screen_config_t *config)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *screens = cJSON_CreateArray();
    if (!root || !screens) {
        cJSON_Delete(root);
        cJSON_Delete(screens);
        return NULL;
    }

    for (size_t i = 0; config && i < config->screen_count; i++) {
        const screen_view_t *view = &config->screens[i];
        cJSON *screen = cJSON_CreateObject();
        cJSON *badges = cJSON_CreateArray();
        cJSON *cards = cJSON_CreateArray();
        if (!screen || !badges || !cards) {
            cJSON_Delete(root);
            cJSON_Delete(screen);
            cJSON_Delete(badges);
            cJSON_Delete(cards);
            return NULL;
        }

        cJSON_AddNumberToObject(screen, "index", view->index);
        cJSON_AddStringToObject(screen, "title", view->title);
        add_optional_string(screen, "icon", view->icon);
        cJSON_AddStringToObject(screen, "type", "sections");

        for (size_t j = 0; j < view->badge_count; j++) {
            cJSON *badge = badge_to_json(&view->badges[j]);
            if (!badge) {
                cJSON_Delete(root);
                return NULL;
            }
            cJSON_AddItemToArray(badges, badge);
        }
        cJSON_AddItemToObject(screen, "badges", badges);

        for (size_t j = 0; j < view->card_count; j++) {
            cJSON *card = card_to_json(&view->cards[j]);
            if (!card) {
                cJSON_Delete(root);
                return NULL;
            }
            cJSON_AddItemToArray(cards, card);
        }
        cJSON_AddItemToObject(screen, "cards", cards);
        cJSON_AddItemToArray(screens, screen);
    }

    cJSON_AddItemToObject(root, "screens", screens);
    return root;
}

void screen_free_config(screen_config_t *config)
{
    if (!config) {
        return;
    }

    for (size_t i = 0; i < config->screen_count; i++) {
        screen_view_t *view = &config->screens[i];
        free(view->title);
        free(view->icon);
        for (size_t j = 0; j < view->badge_count; j++) {
            screen_badge_config_t *badge = &view->badges[j];
            free(badge->data.entity);
            free(badge->data.name);
        }
        free(view->badges);
        for (size_t j = 0; j < view->card_count; j++) {
            screen_card_config_t *card = &view->cards[j];
            if (card->type == SCREEN_CARD_TYPE_HEADING) {
                free(card->data.heading.heading);
            } else {
                screen_tile_card_data_t *tile = &card->data.tile;
                free(tile->entity);
                free(tile->name);
                for (size_t k = 0; k < tile->feature_count; k++) {
                    cJSON_Delete(tile->features[k].data);
                }
                free(tile->features);
            }
        }
        free(view->cards);
    }
    free(config->screens);
    memset(config, 0, sizeof(*config));
}
