#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "littlefs.h"

#include <sys/errno.h>

#include "dirent.h"
#include "esp_littlefs.h"

#define CHUNK_SIZE (4096)

static const char *TAG = "little_file_system";

const esp_vfs_littlefs_conf_t little_fs_config = {
    .base_path = ROOT_PATH,
    .partition_label = "files",
    .format_if_mount_failed = true,
    .dont_mount = false,
};

/**
 * @brief Create a directory in the LittleFS file system.
 *
 * This function checks whether the directory at the specified path already exists.
 * If it does not, the function attempts to create it with read-write-execute (0777) permissions.
 *
 * Logging is performed to indicate whether the directory already exists, was successfully created,
 * or failed to be created.
 *
 * @param path The full path of the directory to be created.
 * @return
 *         - ESP_OK if the directory already exists or was successfully created.
 *         - ESP_FAIL if directory creation failed.
 */
static esp_err_t little_file_system_create_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0777) == 0) {
            ESP_LOGI(TAG, "Directory created successfully: %s", path);
        }
        else {
            ESP_LOGE(TAG, "Directory creation failed: %s", path);
            return ESP_FAIL;
        }
    }
    else {
        ESP_LOGI(TAG, "Directory already exists: %s", path);
    }
    return ESP_OK;
}

/**
 * @brief Initialize the LittleFS file system.
 *
 * Registers and mounts the LittleFS filesystem using the provided configuration. If the mount
 * operation fails, error messages are printed according to the returned error type.
 * If the mount is successful, the partition’s total and used space are logged. If obtaining
 * partition info fails, the function attempts to format the partition.
 * Finally, it attempts to create a default directory defined by CONFIG_PATH.
 *
 * @return ESP_OK when mounting and directory preparation succeed, otherwise
 * the initialization error.
 */
esp_err_t little_file_system_init(void)
{
    size_t total = 0, used = 0;

    /* Register and mount LittleFS */
    esp_err_t ret = esp_vfs_littlefs_register(&little_fs_config);
    if (ret != ESP_OK) {
        switch (ret) {
        case ESP_FAIL:
            ESP_LOGE(TAG, "Failed to mount or format the LittleFS filesystem.");
            break;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(TAG, "LittleFS partition not found.");
            break;
        default:
            ESP_LOGE(TAG, "LittleFS initialization failed: %s", esp_err_to_name(ret));
            break;
        }
        return ret;
    }

    /* Query partition size */
    ret = esp_littlefs_info(little_fs_config.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to get partition info: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Attempting to format the partition...");

        if (esp_littlefs_format(little_fs_config.partition_label) == ESP_OK) {
            ESP_LOGI(TAG, "Partition formatted successfully.");
        }
        else {
            ESP_LOGE(TAG, "Failed to format the LittleFS partition.");
            return ESP_FAIL;
        }
    }
    else {
        ESP_LOGI(TAG, "LittleFS initialized.");
        ESP_LOGI(TAG, "Partition size — Total: %d bytes, Used: %d bytes", total, used);
    }

    /* Create default directory */
    ret = little_file_system_create_dir(CONFIG_PATH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create directory at %s", CONFIG_PATH);
    }
    return ret;
}

/**
 * @brief Delete all regular files in the configuration directory.
 *
 * This function opens the directory specified by CONFIG_PATH and iterates
 * through all entries. If a file is a regular file (not a directory or special file),
 * it constructs the full path and attempts to delete it using `unlink`.
 * Logs are generated for each deletion and any errors encountered.
 */
void little_file_system_delete_all_config_file()
{
    DIR *dir = opendir(CONFIG_PATH);
    struct dirent *entry;
    char file_path[128];

    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", CONFIG_PATH);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            int written = snprintf(file_path, sizeof(file_path), "%s/%s", CONFIG_PATH, entry->d_name);
            if (written < 0 || written >= sizeof(file_path)) {
                ESP_LOGW(TAG, "File path too long, skipping: %s", entry->d_name);
                continue;
            }

            ESP_LOGI(TAG, "Deleting: %s", file_path);

            if (unlink(file_path) != 0) {
                ESP_LOGE(TAG, "Failed to delete file: %s", file_path);
            }
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "All files in %s deleted", CONFIG_PATH);
}

/**
 * @brief Read the contents of a file into a null-terminated string.
 *
 * This function reads the contents of the specified file (within CONFIG_PATH)
 * and returns a dynamically allocated string containing the file's contents.
 * The caller is responsible for freeing the returned buffer.
 *
 * @param file_name The name of the file to read (e.g., "config.json").
 * @return char* Pointer to the file content as a string, or NULL on error.
 */
static char *little_file_system_read_file_to_string(const char *file_name, const char *root_path)
{
    char *full_path = NULL;
    FILE *f = NULL;
    char *buffer = NULL;

    size_t path_len = strlen(root_path) + strlen(file_name) + 2;
    full_path = (char *)malloc(path_len);
    if (!full_path) {
        ESP_LOGE(TAG, "%s: line:%d, malloc for path failed", __FUNCTION__, __LINE__);
        return NULL;
    }

    snprintf(full_path, path_len, "%s/%s", root_path, file_name);

    f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        goto cleanup;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "fseek failed for file: %s", full_path);
        goto cleanup;
    }

    long file_size = ftell(f);
    if (file_size <= 0) {
        ESP_LOGW(TAG, "File is empty or ftell failed: %s", full_path);
        goto cleanup;
    }

    rewind(f);

    buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        ESP_LOGE(TAG, "Memory allocation failed for reading file: %s", full_path);
        goto cleanup;
    }

    size_t read_bytes = fread(buffer, 1, file_size, f);
    if (read_bytes != file_size) {
        ESP_LOGE(TAG, "fread incomplete (%zu/%ld): %s", read_bytes, file_size, full_path);
        free(buffer);
        buffer = NULL;
        goto cleanup;
    }

    buffer[file_size] = '\0';

    ESP_LOGI(TAG, "File read successfully: %s", file_name);

cleanup:
    if (f)
        fclose(f);
    free(full_path);
    return buffer;
}

/**
 * @brief Reads a JSON file from the CONFIG_PATH directory and parses it into a cJSON object.
 *
 * This function first loads the contents of the specified file into memory using
 * `little_file_system_read_file_to_string()`, then parses the content using `cJSON_Parse()`.
 *
 * If the file is empty, missing, or contains invalid JSON, NULL is returned and an error is logged.
 *
 * @param[in] file_name The name of the file to read and parse (e.g., "config.json").
 * @return cJSON* A pointer to a cJSON object representing the parsed JSON structure,
 *                or NULL if an error occurs (e.g., file not found, parse failure).
 *
 * @warning Caller must call cJSON_Delete() to free the returned object.
 * @warning Returns NULL if file_name is NULL, file cannot be read, or JSON is invalid.
 */
cJSON *little_file_system_read_file_to_json(const char *file_name)
{
    if (!file_name) {
        ESP_LOGE(TAG, "Invalid argument: file_name is NULL");
        return NULL;
    }
    char *json_str = little_file_system_read_file_to_string(file_name, CONFIG_PATH);
    if (!json_str) {
        return NULL;
    }
    cJSON *json_obj = cJSON_Parse(json_str);
    if (!json_obj) {
        ESP_LOGE(TAG, "Failed to parse JSON from file: %s", file_name);
    }
    free(json_str);
    return json_obj;
}

/**
 * @brief Write a string to a file within the LittleFS configuration directory.
 *
 * This function creates or overwrites a file located at CONFIG_PATH/file_name
 * with the contents of the provided string.
 *
 * @param file_name The name of the file to write (e.g., "config.json").
 * @param json_str  Null-terminated string to write into the file.
 * @return uint8_t  WRITE_FILE_OK on success, WRITE_FILE_ERR on failure.
 */
static uint8_t little_file_system_write_string_to_file(const char *file_name, char *json_str, const char *root_path)
{
    if (!file_name || !json_str) {
        ESP_LOGE(TAG, "Invalid argument: file_name or json_str is NULL");
        return WRITE_FILE_ERR;
    }

    uint8_t result = WRITE_FILE_ERR;
    FILE *f = NULL;
    char *full_path = NULL;
    size_t path_len = strlen(root_path) + strlen(file_name) + 2;

    full_path = (char *)malloc(path_len);
    if (!full_path) {
        ESP_LOGE(TAG, "%s: line:%d, malloc for path failed", __FUNCTION__, __LINE__);
        return WRITE_FILE_ERR;
    }

    snprintf(full_path, path_len, "%s/%s", root_path, file_name);

    f = fopen(full_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        goto cleanup;
    }

    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, f);
    if (written != len) {
        ESP_LOGE(TAG, "Failed to write full content: %s", full_path);
        goto cleanup;
    }

    if (fflush(f) != 0) {
        ESP_LOGE(TAG, "fflush failed: %s errno=%d (%s)", full_path, errno, strerror(errno));
        goto cleanup;
    }

    if (fsync(fileno(f)) != 0) {
        ESP_LOGE(TAG, "fsync failed: %s errno=%d (%s)", full_path, errno, strerror(errno));
        goto cleanup;
    }

    if (fclose(f) != 0) {
        ESP_LOGE(TAG, "fclose failed: %s errno=%d (%s)", full_path, errno, strerror(errno));
        f = NULL;
        goto cleanup;
    }

    f = NULL;

    result = WRITE_FILE_OK;
    ESP_LOGI(TAG, "File REALLY written successfully: %s", full_path);

cleanup:
    if (f)
        fclose(f);
    free(full_path);
    return result;
}

/**
 * @brief Serialize a cJSON object and save it as a JSON file in the CONFIG_PATH directory.
 *
 * This function converts a cJSON object into a compact JSON string using `cJSON_PrintUnformatted()`
 * and writes the resulting string to a file at CONFIG_PATH/file_path using
 * `little_file_system_write_string_to_file()`.
 *
 * The file is created or overwritten. If serialization or writing fails,
 * appropriate errors are logged and false is returned.
 *
 * @param[in] json_obj Pointer to the cJSON object to serialize and write.
 * @param[in] file_path File name relative to CONFIG_PATH (e.g., "config.json").
 * @return true if the file is successfully written, false otherwise.
 *
 * @warning This function deletes the cJSON object internally using cJSON_Delete().
 *          Do not reuse the json_obj pointer after calling this function.
 */
bool little_file_system_write_json_to_file(cJSON *json_obj, const char *file_path)
{
    if (!json_obj) {
        ESP_LOGE(TAG, "NULL JSON object : %s", file_path);
        return false;
    }

    char *str = cJSON_PrintUnformatted(json_obj);
    if (!str) {
        ESP_LOGE(TAG, "Failed to print JSON to string : %s", file_path);
        cJSON_Delete(json_obj);
        return false;
    }

    int result = little_file_system_write_string_to_file(file_path, str, CONFIG_PATH);

    free(str);
    cJSON_Delete(json_obj);

    if (result != WRITE_FILE_OK) {
        return false;
    }
    return true;
}

/**
 * @brief Reads a JSON file from the ROOT_PATH directory and parses it into a cJSON object.
 *
 * This function first loads the contents of the specified file into memory using
 * `little_file_system_read_file_to_string()`, then parses the content using `cJSON_Parse()`.
 *
 * If the file is empty, missing, or contains invalid JSON, NULL is returned and an error is logged.
 *
 * @param[in] file_name The name of the file to read and parse (e.g., "config.json").
 * @return cJSON* A pointer to a cJSON object representing the parsed JSON structure,
 *                or NULL if an error occurs (e.g., file not found, parse failure).
 *
 * @warning Caller must call cJSON_Delete() to free the returned object.
 * @warning Returns NULL if file_name is NULL, file cannot be read, or JSON is invalid.
 */
cJSON *little_file_system_read_root_file_to_json(const char *file_name)
{
    if (!file_name) {
        ESP_LOGE(TAG, "Invalid argument: file_name is NULL");
        return NULL;
    }
    char *json_str = little_file_system_read_file_to_string(file_name, ROOT_PATH);
    if (!json_str) {
        return NULL;
    }
    cJSON *json_obj = cJSON_Parse(json_str);
    if (!json_obj) {
        ESP_LOGE(TAG, "Failed to parse JSON from file: %s", file_name);
    }
    free(json_str);
    return json_obj;
}

/**
 * @brief Serialize a cJSON object and save it as a JSON file in the ROOT_PATH directory.
 *
 * This function converts a cJSON object into a compact JSON string using `cJSON_PrintUnformatted()`
 * and writes the resulting string to a file at ROOT_PATH/file_path using
 * `little_file_system_write_string_to_file()`.
 *
 * The file is created or overwritten. If serialization or writing fails,
 * appropriate errors are logged and false is returned.
 *
 * @param[in] json_obj Pointer to the cJSON object to serialize and write.
 * @param[in] file_path File name relative to ROOT_PATH (e.g., "config.json").
 * @return true if the file is successfully written, false otherwise.
 *
 * @warning This function deletes the cJSON object internally using cJSON_Delete().
 *          Do not reuse the json_obj pointer after calling this function.
 */
bool little_file_system_write_json_to_root_file(cJSON *json_obj, const char *file_path)
{
    if (!json_obj) {
        ESP_LOGE(TAG, "NULL JSON object : %s", file_path);
        return false;
    }

    char *str = cJSON_PrintUnformatted(json_obj);
    if (!str) {
        ESP_LOGE(TAG, "Failed to print JSON to string : %s", file_path);
        cJSON_Delete(json_obj);
        return false;
    }

    int result = little_file_system_write_string_to_file(file_path, str, ROOT_PATH);

    free(str);
    cJSON_Delete(json_obj);

    if (result != WRITE_FILE_OK) {
        return false;
    }
    return true;
}
