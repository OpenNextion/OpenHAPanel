#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include <esp_http_server.h>
#include "cJSON.h"
#include "littlefs.h"
#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "aes/esp_aes.h"
#include "mbedtls/base64.h"
#include "webserver_method_api.h"
#include "webserver_config_backup.h"
#include "webserver_api.h"

#define BASE64_ENC_RATIO 1.5
#define AES_BLOCK_SIZE 16

ESP_EVENT_DEFINE_BASE(CONFIG_EVENT);

static const char *TAG = "webserver_config_backup";

/**
 * @brief Read entire file into buffer (with null terminator).
 *
 * @param path File path.
 * @param out_buf Pointer to buffer pointer, will be allocated inside.
 * @param out_len Pointer to size_t to store file length.
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
static esp_err_t read_file_to_buffer(const char *path, uint8_t **out_buf, size_t *out_len)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        return ESP_FAIL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    if (length < 0) {
        ESP_LOGE(TAG, "Failed to get file size: %s", path);
        fclose(file);
        return ESP_FAIL;
    }
    fseek(file, 0, SEEK_SET);

    *out_buf = (uint8_t *)malloc(length + 1);
    if (!*out_buf) {
        ESP_LOGE(TAG, "Memory allocation failed for file buffer");
        fclose(file);
        return ESP_FAIL;
    }

    size_t total_read = fread(*out_buf, 1, length, file);
    fclose(file);

    if (total_read != (size_t)length) {
        ESP_LOGE(TAG, "Failed to read complete file: %s", path);
        free(*out_buf);
        return ESP_FAIL;
    }

    (*out_buf)[length] = '\0';
    *out_len = length;
    return ESP_OK;
}

/**
 * @brief List all files (excluding "." and "..") in a directory.
 *
 * @param directory Directory path.
 * @param out_files Array of char pointers to be allocated and filled with filenames.
 * @param out_count Pointer to store number of files found.
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
static esp_err_t list_files_in_directory(const char *directory, char **out_files, size_t *out_count)
{
    DIR *dir = opendir(directory);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", directory);
        return ESP_FAIL;
    }

    struct dirent *entry;
    size_t count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        out_files[count] = strdup(entry->d_name);
        if (!out_files[count]) {
            ESP_LOGE(TAG, "Memory allocation failed for file name");
            closedir(dir);
            return ESP_FAIL;
        }
        count++;
    }
    closedir(dir);
    *out_count = count;
    return ESP_OK;
}

/**
 * @brief Pack config files into a cJSON array object.
 *
 * @param out_json_array Pointer to cJSON* to store created JSON array.
 * @param config_directory Directory path containing config files.
 * @return ESP_OK on success, ESP_FAIL on error.
 */
static esp_err_t pack_config_files_to_json(cJSON **out_json_array, const char *config_directory)
{
    cJSON *json_array = cJSON_CreateArray();
    if (!json_array) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        return ESP_FAIL;
    }

    char *file_list[100];
    size_t file_count = 0;

    esp_err_t err = list_files_in_directory(config_directory, file_list, &file_count);
    if (err != ESP_OK) {
        cJSON_Delete(json_array);
        return err;
    }

    for (size_t i = 0; i < file_count; i++) {
        char path[64];
        snprintf(path, sizeof(path), "%s/%s", config_directory, file_list[i]);

        uint8_t *file_data = NULL;
        size_t file_size = 0;
        err = read_file_to_buffer(path, &file_data, &file_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read file: %s", path);
            for (size_t j = 0; j < file_count; j++)
                free(file_list[j]);
            cJSON_Delete(json_array);
            return err;
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "filename", file_list[i]);
        cJSON_AddNumberToObject(item, "len", file_size);
        cJSON_AddStringToObject(item, "data", (const char *)file_data);
        cJSON_AddItemToArray(json_array, item);

        free(file_data);
        free(file_list[i]);
    }

    *out_json_array = json_array;
    return ESP_OK;
}

/**
 * @brief Parse JSON string representing config files and write to system.
 *
 * @param json_str JSON string containing array of config file info.
 * @return ESP_OK on success, ESP_FAIL on error.
 */
static esp_err_t write_config_from_json_string(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse config JSON");
        return ESP_FAIL;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root)
    {
        const cJSON *filename_obj = cJSON_GetObjectItem(item, "filename");
        const cJSON *len_obj = cJSON_GetObjectItem(item, "len");
        const cJSON *data_obj = cJSON_GetObjectItem(item, "data");

        if (!filename_obj || !len_obj || !data_obj) {
            ESP_LOGW(TAG, "Malformed JSON entry in config");
            continue;
        }

        const char *filename = filename_obj->valuestring;
        size_t len = (size_t)len_obj->valueint;
        const char *data = data_obj->valuestring;

        char path[64];
        snprintf(path, sizeof(path), "%s/%s", CONFIG_PATH, filename);

        FILE *file = fopen(path, "wb");
        if (!file) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
            continue;
        }

        fwrite(data, 1, len, file);
        fclose(file);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief Retrieve AES key derived from MAC address.
 *
 * @param out_key Buffer (at least 16 bytes) to store the AES key.
 * @return 0 on success, -1 on failure.
 */
static int8_t derive_aes_key(uint8_t *out_key)
{
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_BASE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address for AES key");
        return -1;
    }

    memcpy(out_key, "AES128KEY", 9); // including '\0'
    memcpy(out_key + 9, mac, 6);
    return 0;
}

/**
 * @brief Retrieve AES IV derived from MAC address.
 *
 * @param out_iv Buffer (at least 16 bytes) to store the AES IV.
 * @return 0 on success, -1 on failure.
 */
static int8_t derive_aes_iv(uint8_t *out_iv)
{
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_BASE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address for AES IV");
        return -1;
    }

    memcpy(out_iv, "AES128IV", 8); // including '\0'
    memcpy(out_iv + 8, mac, 6);
    return 0;
}

/**
 * @brief Encrypt data buffer in-place using AES-128-CBC with PKCS#7 padding.
 *
 * @param data Buffer containing data to encrypt, must have space for padding.
 * @param data_len Length of original data (before padding).
 * @return Length of encrypted data on success, -1 on failure.
 */
static int32_t aes128_cbc_encrypt_inplace(uint8_t *data, uint32_t data_len)
{
    int pad_len = 16 - (data_len % 16);
    for (int i = 0; i < pad_len; i++) {
        data[data_len + i] = pad_len;
    }
    uint32_t padded_len = data_len + pad_len;

    uint8_t key[16] = {0}, iv[16] = {0};
    if (derive_aes_key(key) != 0 || derive_aes_iv(iv) != 0) {
        return -1;
    }

    esp_aes_context ctx;
    esp_aes_init(&ctx);
    if (esp_aes_setkey(&ctx, key, 128) != 0) {
        esp_aes_free(&ctx);
        return -1;
    }
    esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, padded_len, iv, data, data);
    esp_aes_free(&ctx);

    return padded_len;
}

/**
 * @brief Decrypt data buffer in-place using AES-128-CBC.
 *
 * @param data Buffer containing encrypted data.
 * @param data_len Length of encrypted data.
 * @return Length of decrypted data on success, -1 on failure.
 */
static int32_t aes128_cbc_decrypt_inplace(uint8_t *data, uint32_t data_len)
{
    uint8_t key[16] = {0}, iv[16] = {0};
    if (derive_aes_key(key) != 0 || derive_aes_iv(iv) != 0) {
        return -1;
    }

    esp_aes_context ctx;
    esp_aes_init(&ctx);
    if (esp_aes_setkey(&ctx, key, 128) != 0) {
        esp_aes_free(&ctx);
        return -1;
    }
    if (esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, data_len, iv, data, data) != 0) {
        esp_aes_free(&ctx);
        return -1;
    }
    esp_aes_free(&ctx);

    // Remove PKCS#7 padding
    int pad_len = data[data_len - 1];
    if (pad_len <= 0 || pad_len > 16) {
        return -1;
    }
    return data_len - pad_len;
}

/**
 * @brief Collect and encrypt system configuration into a binary buffer.
 *
 * This function:
 * - Uses `pack_config_files_to_json()` to gather configuration files from CONFIG_PATH
 *   and serialize them into a cJSON object.
 * - Converts the JSON object into an unformatted string.
 * - Allocates and copies the string into a buffer, with extra space for PKCS7 padding.
 * - Encrypts the buffer in-place using AES-128-CBC via `aes128_cbc_encrypt_inplace()`.
 * - Returns the encrypted buffer and its length via the out parameter.
 *
 * If any step fails (e.g., memory allocation, JSON serialization, encryption),
 * it returns NULL and logs the appropriate error.
 *
 * @param[out] out_size Pointer to receive the size of the encrypted output.
 *
 * @return A heap-allocated pointer to the encrypted buffer, or NULL on failure.
 *         The caller must free the returned buffer when done.
 */
uint8_t *webserver_config_backup_get_encrypted_sys_config(int32_t *out_size)
{
    cJSON *json_config = NULL;
    if (pack_config_files_to_json(&json_config, CONFIG_PATH) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pack config files into JSON");
        return NULL;
    }

    char *json_str = cJSON_PrintUnformatted(json_config);
    cJSON_Delete(json_config);

    if (!json_str) {
        ESP_LOGE(TAG, "Failed to print JSON config");
        return NULL;
    }

    ESP_LOGI(TAG, "Backup JSON: %s", json_str);

    size_t json_len = strlen(json_str);
    size_t buf_len = ((json_len / 16) + 2) * 16; // make space for padding

    uint8_t *enc_buf = malloc(buf_len);
    if (!enc_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for encrypted data");
        free(json_str);
        return NULL;
    }
    memset(enc_buf, 0, buf_len);
    memcpy(enc_buf, json_str, json_len);
    free(json_str);

    int32_t encrypted_len = aes128_cbc_encrypt_inplace(enc_buf, json_len);
    if (encrypted_len < 0) {
        ESP_LOGE(TAG, "AES encryption failed");
        free(enc_buf);
        return NULL;
    }

    *out_size = encrypted_len;
    return enc_buf;
}

/**
 * @brief Handle system configuration backup download request via WebSocket API.
 *
 * This function performs the following steps:
 * - Calls `webserver_config_backup_get_encrypted_sys_config()` to retrieve the current
 *   encrypted system configuration and its length.
 * - Encodes the encrypted binary data into a Base64 string.
 * - Constructs a JSON object containing the encoded string under the "content" key.
 * - Sends the response back to the client via WebSocket with the appropriate method and sequence ID.
 *
 * If any step fails (e.g., memory allocation, Base64 encoding), it logs the error and
 * returns an appropriate failure status.
 *
 * @param[in] data           Reserved input, currently unused but required for API compatibility.
 * @param[in] now_resp_arg   Contains response metadata including the WebSocket sequence number.
 *
 * @return ESP_OK on success, or error codes such as ESP_FAIL / ESP_ERR_NO_MEM on failure.
 */
esp_err_t method_api_handle_download_backup(cJSON *data, async_resp_arg *now_resp_arg)
{
    int32_t encrypted_len = 0;
    uint8_t *encrypted_data = webserver_config_backup_get_encrypted_sys_config(&encrypted_len);
    if (!encrypted_data) {
        ESP_LOGE(TAG, "Failed to get encrypted system config");
        return ESP_FAIL;
    }

    size_t base64_buf_size = (size_t)(encrypted_len * BASE64_ENC_RATIO + 4);
    uint8_t *base64_buf = (uint8_t *)malloc(base64_buf_size);
    if (!base64_buf) {
        ESP_LOGE(TAG, "Failed to allocate base64 buffer");
        free(encrypted_data);
        return ESP_ERR_NO_MEM;
    }

    memset(base64_buf, 0, base64_buf_size);
    size_t base64_len = 0;
    if (mbedtls_base64_encode(base64_buf, base64_buf_size, &base64_len, encrypted_data, encrypted_len) != 0) {
        ESP_LOGE(TAG, "Base64 encoding failed");
        free(encrypted_data);
        free(base64_buf);
        return ESP_FAIL;
    }
    cJSON *data_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(data_obj, "content", (char *)base64_buf);
    webserver_data_code_response_to_current_client(data_obj, WEB_API_METHOD_DOWNLOAD_BACKUP_RSP, now_resp_arg->seq,
                                                   WEB_API_ERROR_OK);

    free(encrypted_data);
    free(base64_buf);
    return ESP_OK;
}

/**
 * @brief Handle upload of encrypted system configuration backup via WebSocket API.
 *
 * This function performs the following steps:
 * - Extracts the Base64-encoded "content" string from the input JSON.
 * - Allocates memory and decodes the content from Base64.
 * - Decrypts the decoded buffer using AES-128-CBC.
 * - Validates PKCS7 padding and checks for essential JSON keys ("filename", "len", "data").
 * - Writes the parsed configuration to the system.
 * - Posts an event to notify the system of the configuration upload.
 * - Sends a success or error response back to the WebSocket client.
 *
 * Errors such as malformed input, decoding/decryption failure, or invalid data structure
 * are logged and will result in a failure response.
 *
 * @param[in] data           JSON object containing the Base64-encoded encrypted config ("content").
 * @param[in] now_resp_arg   Response arguments for sending back the WebSocket response.
 *
 * @return ESP_OK on success, ESP_FAIL on error.
 */
esp_err_t method_api_handle_upload_backup(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error_code = WEB_API_ERROR_FAIL;
    cJSON *content = cJSON_GetObjectItem(data, "content");
    if (!content || !cJSON_IsString(content)) {
        ESP_LOGE(TAG, "Invalid or missing 'content' field");
        goto send_response;
    }

    size_t encoded_len = strlen(content->valuestring);
    size_t decoded_buf_len = encoded_len / 4 * 3 + 4;
    uint8_t *decoded_buf = (uint8_t *)malloc(decoded_buf_len);
    if (!decoded_buf) {
        ESP_LOGE(TAG, "Failed to allocate decoded buffer");
        goto send_response;
    }

    size_t decoded_len = 0;
    if (mbedtls_base64_decode(decoded_buf, decoded_buf_len, &decoded_len, (const uint8_t *)content->valuestring,
                              encoded_len) != 0) {
        ESP_LOGE(TAG, "Base64 decoding failed");
        goto cleanup;
    }

    if (decoded_len < AES_BLOCK_SIZE || decoded_len % AES_BLOCK_SIZE != 0) {
        ESP_LOGE(TAG, "Invalid AES block size");
        goto cleanup;
    }

    if (aes128_cbc_decrypt_inplace(decoded_buf, decoded_len) < 0) {
        ESP_LOGE(TAG, "AES decryption failed");
        error_code = LFS_ERR_NO_DEVICE_BACKUP_FILE;
        goto cleanup;
    }

    // PKCS7 padding validation
    uint8_t pad_len = decoded_buf[decoded_len - 1];
    if (pad_len > AES_BLOCK_SIZE) {
        ESP_LOGE(TAG, "Invalid PKCS7 padding value: %d", pad_len);
        goto cleanup;
    }

    decoded_len -= pad_len;
    decoded_buf[decoded_len] = '\0';

    if (!strstr((char *)decoded_buf, "filename") || !strstr((char *)decoded_buf, "len") ||
        !strstr((char *)decoded_buf, "data")) {
        error_code = LFS_ERR_NO_DEVICE_BACKUP_FILE;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Parsed config: %s", decoded_buf);
    if (write_config_from_json_string((char *)decoded_buf) < 0) {
        ESP_LOGE(TAG, "Failed to write config");
        goto cleanup;
    }

    esp_event_post(CONFIG_EVENT, CONFIG_EVENT_UPLOAD, NULL, 0, portMAX_DELAY);
    error_code = WEB_API_ERROR_OK;

cleanup:
    free(decoded_buf);

send_response:
    webserver_code_response_to_current_client(error_code, WEB_API_METHOD_UPLOAD_BACKUP_RSP, now_resp_arg->seq);
    return error_code == WEB_API_ERROR_OK ? ESP_OK : ESP_FAIL;
}