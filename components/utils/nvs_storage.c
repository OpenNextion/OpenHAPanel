#include "nvs_storage.h"

#include <stdbool.h>

static bool write_item_is_valid(const nvs_storage_write_item_t *item)
{
    if (!item || !item->key || item->key[0] == '\0') {
        return false;
    }

    if (item->type == NVS_STORAGE_TYPE_STRING) {
        return item->value.string != NULL;
    }

    return item->type == NVS_STORAGE_TYPE_U8 || item->type == NVS_STORAGE_TYPE_U32;
}

static bool read_item_is_valid(const nvs_storage_read_item_t *item)
{
    if (!item || !item->key || item->key[0] == '\0' || !item->value) {
        return false;
    }

    if (item->type == NVS_STORAGE_TYPE_STRING) {
        return item->value_size > 0;
    }

    return item->type == NVS_STORAGE_TYPE_U8 || item->type == NVS_STORAGE_TYPE_U32;
}

static esp_err_t write_item(nvs_handle_t handle, const nvs_storage_write_item_t *item)
{
    switch (item->type) {
    case NVS_STORAGE_TYPE_U8:
        return nvs_set_u8(handle, item->key, item->value.u8);
    case NVS_STORAGE_TYPE_U32:
        return nvs_set_u32(handle, item->key, item->value.u32);
    case NVS_STORAGE_TYPE_STRING:
        return nvs_set_str(handle, item->key, item->value.string);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static esp_err_t read_item(nvs_handle_t handle, nvs_storage_read_item_t *item)
{
    switch (item->type) {
    case NVS_STORAGE_TYPE_U8:
        return nvs_get_u8(handle, item->key, (uint8_t *)item->value);
    case NVS_STORAGE_TYPE_U32:
        return nvs_get_u32(handle, item->key, (uint32_t *)item->value);
    case NVS_STORAGE_TYPE_STRING: {
        size_t length = item->value_size;
        return nvs_get_str(handle, item->key, (char *)item->value, &length);
    }
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t nvs_storage_write(const char *namespace_name, const nvs_storage_write_item_t *items, size_t item_count)
{
    if (!namespace_name || namespace_name[0] == '\0' || !items || item_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < item_count; i++) {
        if (!write_item_is_valid(&items[i])) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    for (size_t i = 0; i < item_count && ret == ESP_OK; i++) {
        ret = write_item(handle, &items[i]);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

esp_err_t nvs_storage_read(const char *namespace_name, nvs_storage_read_item_t *items, size_t item_count)
{
    if (!namespace_name || namespace_name[0] == '\0' || !items || item_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < item_count; i++) {
        if (!read_item_is_valid(&items[i])) {
            return ESP_ERR_INVALID_ARG;
        }
        items[i].result = ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    for (size_t i = 0; i < item_count; i++) {
        items[i].result = read_item(handle, &items[i]);
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t nvs_storage_set_u8(const char *namespace_name, const char *key, uint8_t value)
{
    const nvs_storage_write_item_t item = {
        .key = key,
        .type = NVS_STORAGE_TYPE_U8,
        .value.u8 = value,
    };
    return nvs_storage_write(namespace_name, &item, 1);
}

esp_err_t nvs_storage_get_u8(const char *namespace_name, const char *key, uint8_t *value)
{
    nvs_storage_read_item_t item = {
        .key = key,
        .type = NVS_STORAGE_TYPE_U8,
        .value = value,
    };
    esp_err_t ret = nvs_storage_read(namespace_name, &item, 1);
    return ret == ESP_OK ? item.result : ret;
}

esp_err_t nvs_storage_set_u32(const char *namespace_name, const char *key, uint32_t value)
{
    const nvs_storage_write_item_t item = {
        .key = key,
        .type = NVS_STORAGE_TYPE_U32,
        .value.u32 = value,
    };
    return nvs_storage_write(namespace_name, &item, 1);
}

esp_err_t nvs_storage_get_u32(const char *namespace_name, const char *key, uint32_t *value)
{
    nvs_storage_read_item_t item = {
        .key = key,
        .type = NVS_STORAGE_TYPE_U32,
        .value = value,
    };
    esp_err_t ret = nvs_storage_read(namespace_name, &item, 1);
    return ret == ESP_OK ? item.result : ret;
}

esp_err_t nvs_storage_set_string(const char *namespace_name, const char *key, const char *value)
{
    const nvs_storage_write_item_t item = {
        .key = key,
        .type = NVS_STORAGE_TYPE_STRING,
        .value.string = value,
    };
    return nvs_storage_write(namespace_name, &item, 1);
}

esp_err_t nvs_storage_get_string(const char *namespace_name, const char *key, char *value, size_t value_size)
{
    nvs_storage_read_item_t item = {
        .key = key,
        .type = NVS_STORAGE_TYPE_STRING,
        .value = value,
        .value_size = value_size,
    };
    esp_err_t ret = nvs_storage_read(namespace_name, &item, 1);
    return ret == ESP_OK ? item.result : ret;
}

esp_err_t nvs_storage_erase_namespace(const char *namespace_name)
{
    if (!namespace_name || namespace_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_erase_all(handle);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}
