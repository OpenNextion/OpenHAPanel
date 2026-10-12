#include "screen_validate.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "screen_validate";

static bool is_optional_null(const cJSON *item)
{
    return !item || cJSON_IsNull(item);
}

static bool is_string_value(const cJSON *item)
{
    return cJSON_IsString(item) && item->valuestring;
}

static bool string_in_list(const char *value, const char *const *allowed, size_t allowed_len)
{
    for (size_t i = 0; i < allowed_len; i++) {
        if (strcmp(value, allowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t validate_optional_string(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (is_optional_null(item) || is_string_value(item)) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Invalid optional string: %s", key);
    return ESP_FAIL;
}

static esp_err_t validate_optional_bool(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (is_optional_null(item) || cJSON_IsBool(item)) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Invalid optional bool: %s", key);
    return ESP_FAIL;
}

static esp_err_t validate_action_item(const cJSON *root, const char *key)
{
    const char *allowed[] = {"toggle", "more-info", "nothing"};
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (is_optional_null(item)) {
        return ESP_OK;
    }
    if (!cJSON_IsObject(item)) {
        ESP_LOGE(TAG, "Invalid action object: %s", key);
        return ESP_FAIL;
    }

    const cJSON *action = cJSON_GetObjectItemCaseSensitive(item, "action");
    if (is_optional_null(action)) {
        return ESP_OK;
    }
    if (!is_string_value(action) || !string_in_list(action->valuestring, allowed, sizeof(allowed) / sizeof(allowed[0]))) {
        ESP_LOGE(TAG, "Invalid action value: %s", key);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t validate_action_config(const cJSON *root)
{
    const cJSON *action_config = cJSON_GetObjectItemCaseSensitive(root, "actionConfig");
    if (is_optional_null(action_config)) {
        return ESP_OK;
    }
    if (!cJSON_IsObject(action_config)) {
        ESP_LOGE(TAG, "Invalid actionConfig");
        return ESP_FAIL;
    }

    if (validate_action_item(action_config, "tap_action") != ESP_OK ||
        validate_action_item(action_config, "hold_action") != ESP_OK ||
        validate_action_item(action_config, "icon_tap_action") != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t validate_badge_data(const cJSON *data)
{
    if (is_optional_null(data)) {
        return ESP_OK;
    }
    if (!cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "Invalid badge data");
        return ESP_FAIL;
    }

    const cJSON *entity = cJSON_GetObjectItemCaseSensitive(data, "entity");
    if (!is_string_value(entity)) {
        ESP_LOGE(TAG, "Missing or invalid badge entity");
        return ESP_FAIL;
    }

    if (validate_optional_string(data, "name") != ESP_OK || validate_action_config(data) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t validate_badge(const cJSON *badge)
{
    if (!cJSON_IsObject(badge)) {
        ESP_LOGE(TAG, "Invalid badge");
        return ESP_FAIL;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(badge, "type");
    if (!is_string_value(type) || strcmp(type->valuestring, "entity") != 0) {
        ESP_LOGE(TAG, "Missing or invalid badge type");
        return ESP_FAIL;
    }

    return validate_badge_data(cJSON_GetObjectItemCaseSensitive(badge, "data"));
}

static esp_err_t validate_badges(const cJSON *screen)
{
    const cJSON *badges = cJSON_GetObjectItemCaseSensitive(screen, "badges");
    if (!cJSON_IsArray(badges)) {
        ESP_LOGE(TAG, "Missing or invalid badges");
        return ESP_FAIL;
    }

    const cJSON *badge = NULL;
    cJSON_ArrayForEach(badge, badges)
    {
        if (validate_badge(badge) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t validate_feature(const cJSON *feature)
{
    const char *allowed[] = {"light-brightness", "light-color-temp", "cover-open-close", "cover-position"};
    if (!cJSON_IsObject(feature)) {
        ESP_LOGE(TAG, "Invalid feature");
        return ESP_FAIL;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(feature, "type");
    if (!is_string_value(type) || !string_in_list(type->valuestring, allowed, sizeof(allowed) / sizeof(allowed[0]))) {
        ESP_LOGE(TAG, "Missing or invalid feature type");
        return ESP_FAIL;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(feature, "data");
    if (!is_optional_null(data) && !cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "Invalid feature data");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t validate_features(const cJSON *data)
{
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(data, "features");
    if (is_optional_null(features)) {
        return ESP_OK;
    }
    if (!cJSON_IsArray(features)) {
        ESP_LOGE(TAG, "Invalid features");
        return ESP_FAIL;
    }

    const cJSON *feature = NULL;
    cJSON_ArrayForEach(feature, features)
    {
        if (validate_feature(feature) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t validate_heading_card_data(const cJSON *data)
{
    const cJSON *heading = cJSON_GetObjectItemCaseSensitive(data, "heading");
    if (!is_string_value(heading)) {
        ESP_LOGE(TAG, "Missing or invalid heading");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t validate_tile_card_data(const cJSON *data)
{
    const cJSON *entity = cJSON_GetObjectItemCaseSensitive(data, "entity");
    if (!is_string_value(entity)) {
        ESP_LOGE(TAG, "Missing or invalid tile entity");
        return ESP_FAIL;
    }

    if (validate_optional_string(data, "name") != ESP_OK || validate_optional_bool(data, "full_width") != ESP_OK ||
        validate_action_config(data) != ESP_OK || validate_features(data) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t validate_card(const cJSON *card)
{
    if (!cJSON_IsObject(card)) {
        ESP_LOGE(TAG, "Invalid card");
        return ESP_FAIL;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(card, "type");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(card, "data");
    if (!is_string_value(type) || !cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "Missing or invalid card type/data");
        return ESP_FAIL;
    }

    if (strcmp(type->valuestring, "heading") == 0) {
        return validate_heading_card_data(data);
    }
    if (strcmp(type->valuestring, "tile") == 0) {
        return validate_tile_card_data(data);
    }

    ESP_LOGE(TAG, "Unsupported card type: %s", type->valuestring);
    return ESP_FAIL;
}

static esp_err_t validate_cards(const cJSON *screen)
{
    const cJSON *cards = cJSON_GetObjectItemCaseSensitive(screen, "cards");
    if (!cJSON_IsArray(cards)) {
        ESP_LOGE(TAG, "Missing or invalid cards");
        return ESP_FAIL;
    }

    const cJSON *card = NULL;
    cJSON_ArrayForEach(card, cards)
    {
        if (validate_card(card) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t validate_screen(const cJSON *screen)
{
    if (!cJSON_IsObject(screen)) {
        ESP_LOGE(TAG, "Invalid screen");
        return ESP_FAIL;
    }

    const cJSON *index = cJSON_GetObjectItemCaseSensitive(screen, "index");
    const cJSON *title = cJSON_GetObjectItemCaseSensitive(screen, "title");
    if (!cJSON_IsNumber(index) || index->valuedouble != (double)index->valueint || !is_string_value(title)) {
        ESP_LOGE(TAG, "Missing or invalid screen index/title");
        return ESP_FAIL;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(screen, "type");
    if (!is_optional_null(type) && (!is_string_value(type) || strcmp(type->valuestring, "sections") != 0)) {
        ESP_LOGE(TAG, "Invalid screen type");
        return ESP_FAIL;
    }

    if (validate_optional_string(screen, "icon") != ESP_OK || validate_badges(screen) != ESP_OK ||
        validate_cards(screen) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t screen_validate_config(const cJSON *config)
{
    if (!cJSON_IsObject(config)) {
        ESP_LOGE(TAG, "Invalid screen config root");
        return ESP_FAIL;
    }

    const cJSON *screens = cJSON_GetObjectItemCaseSensitive(config, "screens");
    if (!cJSON_IsArray(screens)) {
        ESP_LOGE(TAG, "Missing or invalid screens");
        return ESP_FAIL;
    }

    const cJSON *screen = NULL;
    cJSON_ArrayForEach(screen, screens)
    {
        if (validate_screen(screen) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}
