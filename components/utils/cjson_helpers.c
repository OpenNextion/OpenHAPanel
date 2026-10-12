#include "esp_log.h"
#include "cJSON.h"
#include "cjson_helpers.h"

#define WEBSERVER_UTILS_TAG "webserver_utils"

/**
 * @brief Parse a string field from a cJSON object
 *
 * This function retrieves a string value associated with the specified key
 * from the given cJSON object.
 *
 * @param root  Pointer to the cJSON object to parse.
 * @param key   The key string to search for in the cJSON object.
 * @param out   Pointer to a char* where the string value will be stored.
 *
 * @return
 *      - true on success
 *      - false if the key is not found or the value is not a string
 */
bool cjson_parse_string_field(cJSON *root, const char *key, char **out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item) {
        ESP_LOGW(WEBSERVER_UTILS_TAG, "%s parse error", key);
        return false;
    }

    *out = cJSON_GetStringValue(item);
    if (!*out) {
        ESP_LOGE(WEBSERVER_UTILS_TAG, "%s string parse error", key);
        return false;
    }

    return true;
}

/**
 * @brief Parse an integer field from a cJSON object
 *
 * This function retrieves an integer value associated with the specified key
 * from the given cJSON object.
 *
 * @param root  Pointer to the cJSON object to parse.
 * @param key   The key string to search for in the cJSON object.
 * @param out   Pointer to an int where the integer value will be stored.
 *
 * @return
 *      - true on success
 *      - false if the key is not found or the value is not a number
 */
bool cjson_parse_int_field(cJSON *root, const char *key, int *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item || !cJSON_IsNumber(item)) {
        ESP_LOGW(WEBSERVER_UTILS_TAG, "%s parse error", key);
        return false;
    }

    *out = item->valueint;
    return true;
}

/**
 * @brief Parse an integer field from a cJSON object
 *
 * This function retrieves an integer value associated with the specified key
 * from the given cJSON object.
 *
 * @param root  Pointer to the cJSON object to parse.
 * @param key   The key string to search for in the cJSON object.
 * @param out   Pointer to an int where the integer value will be stored.
 *
 * @return
 *      - true on success
 *      - false if the key is not found or the value is not a number
 */
bool cjson_parse_uint32_field(cJSON *root, const char *key, uint32_t *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item) {
        ESP_LOGW(WEBSERVER_UTILS_TAG, "%s parse error", key);
        return false;
    }

    *out = (uint32_t)cJSON_GetNumberValue(item);
    return true;
}

/**
 * @brief Parse a double (floating point) field from a cJSON object
 *
 * This function retrieves a double value associated with the specified key
 * from the given cJSON object.
 *
 * @param root  Pointer to the cJSON object to parse.
 * @param key   The key string to search for in the cJSON object.
 * @param out   Pointer to a double where the double value will be stored.
 *
 * @return
 *      - true on success
 *      - false if the key is not found or the value is not a number
 */
bool cjson_parse_double_field(cJSON *root, const char *key, double *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item || !cJSON_IsNumber(item)) {
        ESP_LOGW(WEBSERVER_UTILS_TAG, "%s parse error", key);
        return false;
    }

    *out = item->valuedouble;
    return true;
}

/**
 * @brief Parse a boolean field from a cJSON object
 *
 * This function retrieves a boolean value associated with the specified key
 * from the given cJSON object.
 *
 * @param root  Pointer to the cJSON object to parse.
 * @param key   The key string to search for in the cJSON object.
 * @param out   Pointer to an int where the boolean value will be stored (0 or 1).
 *
 * @return
 *      - true on success
 *      - false if the key is not found or the value is not a boolean
 */
bool cjson_parse_bool_field(cJSON *root, const char *key, bool *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item || !(cJSON_IsBool(item))) {
        ESP_LOGW(WEBSERVER_UTILS_TAG, "%s parse error", key);
        return false;
    }

    *out = cJSON_IsTrue(item);
    return true;
}

/**
 * @brief Parse an object field from a cJSON object
 *
 * This function retrieves a nested cJSON object associated with the specified key
 * from the given cJSON object.
 *
 * @param root  Pointer to the cJSON object to parse.
 * @param key   The key string to search for in the cJSON object.
 * @param out   Pointer to a cJSON* where the nested object will be stored.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL if the key is not found or the value is not an object
 */
bool cjson_parse_object_field(cJSON *root, const char *key, cJSON **out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item || !cJSON_IsObject(item)) {
        ESP_LOGW(WEBSERVER_UTILS_TAG, "%s parse error", key);
        return false;
    }

    *out = item;
    return true;
}

bool parse_optional_bool(cJSON *data, const char *key, bool *value, bool *present)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(data, key);
    if (!item) {
        *present = false;
        return true;
    }
    *present = true;
    if (!cJSON_IsBool(item)) {
        return false;
    }
    *value = cJSON_IsTrue(item);
    return true;
}

bool parse_optional_uint32(cJSON *data, const char *key, uint32_t min_value,
                           uint32_t max_value, uint32_t *value, bool *present)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(data, key);
    if (!item) {
        *present = false;
        return true;
    }
    *present = true;
    if (!cJSON_IsNumber(item) || item->valuedouble < min_value || item->valuedouble > max_value) {
        return false;
    }
    *value = (uint32_t)item->valuedouble;
    return true;
}
