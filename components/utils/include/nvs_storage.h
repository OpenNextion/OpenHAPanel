#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_UTILS_NVS_STORAGE_H
#define OPEN_NEXTION_COMPONENTS_UTILS_NVS_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include "nvs.h"

typedef enum
{
    NVS_STORAGE_TYPE_U8,
    NVS_STORAGE_TYPE_U32,
    NVS_STORAGE_TYPE_STRING,
} nvs_storage_type_t;

typedef struct
{
    const char *key;
    nvs_storage_type_t type;
    union
    {
        uint8_t u8;
        uint32_t u32;
        const char *string;
    } value;
} nvs_storage_write_item_t;

typedef struct
{
    const char *key;
    nvs_storage_type_t type;
    void *value;
    size_t value_size;
    esp_err_t result;
} nvs_storage_read_item_t;

/**
 * @brief Atomically write one or more values to an NVS namespace.
 *
 * All items are written using a single NVS handle and committed once. If any
 * item fails, the function closes the handle without committing.
 */
esp_err_t nvs_storage_write(const char *namespace_name, const nvs_storage_write_item_t *items, size_t item_count);

/**
 * @brief Read one or more values from an NVS namespace.
 *
 * The function return value reports namespace/argument errors. The result of
 * each key lookup is stored in the corresponding item's result field, allowing
 * callers to provide defaults for optional keys.
 */
esp_err_t nvs_storage_read(const char *namespace_name, nvs_storage_read_item_t *items, size_t item_count);

esp_err_t nvs_storage_set_u8(const char *namespace_name, const char *key, uint8_t value);
esp_err_t nvs_storage_get_u8(const char *namespace_name, const char *key, uint8_t *value);
esp_err_t nvs_storage_set_u32(const char *namespace_name, const char *key, uint32_t value);
esp_err_t nvs_storage_get_u32(const char *namespace_name, const char *key, uint32_t *value);
esp_err_t nvs_storage_set_string(const char *namespace_name, const char *key, const char *value);
esp_err_t nvs_storage_get_string(const char *namespace_name, const char *key, char *value, size_t value_size);

/**
 * @brief Erase and commit all keys in an NVS namespace.
 */
esp_err_t nvs_storage_erase_namespace(const char *namespace_name);

#endif

#ifdef __cplusplus
}
#endif
