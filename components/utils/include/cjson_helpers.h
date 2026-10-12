#ifndef OPEN_NEXTION_COMPONENTS_UTILS_INCLUDE_CJSON_HELPERS_H
#define OPEN_NEXTION_COMPONENTS_UTILS_INCLUDE_CJSON_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_check.h"
#include "cJSON.h"

/**
 * @brief Parse a string field from a cJSON object
 *
 * Retrieves the string value associated with the specified key from the given cJSON object.
 *
 * @param root Pointer to the cJSON object to parse.
 * @param key  The key string to search for in the cJSON object.
 * @param out  Pointer to a char* where the string value will be stored.
 *
 * @return
 *      - true on success
 *      - false if key is not found or value is not a string
 */
bool cjson_parse_string_field(cJSON* root, const char* key, char** out);

/**
 * @brief Parse an integer field from a cJSON object
 *
 * Retrieves the integer value associated with the specified key from the given cJSON object.
 *
 * @param root Pointer to the cJSON object to parse.
 * @param key  The key string to search for in the cJSON object.
 * @param out  Pointer to an int where the integer value will be stored.
 *
 * @return
 *      - true on success
 *      - false if key is not found or value is not a number
 */
bool cjson_parse_int_field(cJSON* root, const char* key, int* out);

/**
 * @brief Parse an integer field from a cJSON object
 *
 * Retrieves the integer value associated with the specified key from the given cJSON object.
 *
 * @param root Pointer to the cJSON object to parse.
 * @param key  The key string to search for in the cJSON object.
 * @param out  Pointer to an int where the integer value will be stored.
 *
 * @return
 *      - true on success
 *      - false if key is not found or value is not a number
 */
bool cjson_parse_uint32_field(cJSON* root, const char* key, uint32_t* out);

/**
 * @brief Parse a double field from a cJSON object
 *
 * Retrieves the double (floating point) value associated with the specified key
 * from the given cJSON object.
 *
 * @param root Pointer to the cJSON object to parse.
 * @param key  The key string to search for in the cJSON object.
 * @param out  Pointer to a double where the value will be stored.
 *
 * @return
 *      - true on success
 *      - false if key is not found or value is not a number
 */
bool cjson_parse_double_field(cJSON* root, const char* key, double* out);

/**
 * @brief Parse a boolean field from a cJSON object
 *
 * Retrieves the boolean value associated with the specified key from the given cJSON object.
 *
 * @param root Pointer to the cJSON object to parse.
 * @param key  The key string to search for in the cJSON object.
 * @param out  Pointer to an int where the boolean value (0 or 1) will be stored.
 *
 * @return
 *      - true on success
 *      - false if key is not found or value is not a boolean
 */
bool cjson_parse_bool_field(cJSON* root, const char* key, bool* out);

/**
 * @brief Parse an object field from a cJSON object
 *
 * Retrieves the nested cJSON object associated with the specified key from the given cJSON object.
 *
 * @param root Pointer to the cJSON object to parse.
 * @param key  The key string to search for in the cJSON object.
 * @param out  Pointer to a cJSON* where the nested object will be stored.
 *
 * @return
 *      - true on success
 *      - false if key is not found or value is not an object
 */
bool cjson_parse_object_field(cJSON* root, const char* key, cJSON** out);

bool parse_optional_bool(cJSON* data, const char* key, bool* value, bool* present);

bool parse_optional_uint32(cJSON* data,
	const char* key,
	uint32_t min_value,
	uint32_t max_value,
	uint32_t* value,
	bool* present);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_UTILS_INCLUDE_CJSON_HELPERS_H
