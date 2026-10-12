#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_LFS_INCLUDE_LITTLE_FILE_SYSTEM_H
#define OPEN_NEXTION_COMPONENTS_LFS_INCLUDE_LITTLE_FILE_SYSTEM_H

#include <stdbool.h>
#include "cJSON.h"
#include "esp_err.h"

#define WRITE_FILE_ERR 1 /* failed to write to file */
#define WRITE_FILE_OK 0  /* success to write to file */

#define ROOT_PATH "/Littlefs"
#define CONFIG_PATH "/Littlefs/Configs"

/**
 * @brief Initialize the LittleFS file system.
 *
 * This function attempts to mount the LittleFS file system using the provided configuration.
 * If mounting fails, appropriate error messages are logged. If the partition is successfully
 * mounted, its total and used size is logged. If querying partition info fails, it attempts to format it.
 *
 * @return ESP_OK when the filesystem is mounted and the configuration
 * directory is available; otherwise the initialization error.
 */
esp_err_t little_file_system_init(void);

/**
 * @brief Delete all regular files in the configuration directory.
 *
 * This function opens the directory defined by CONFIG_PATH and deletes
 * all regular files found inside. It logs each deletion attempt and any errors.
 */
void little_file_system_delete_all_config_file();

/**
 * @brief Reads a JSON file and parses it into a cJSON object.
 *
 * This function reads the contents of a file located in CONFIG_PATH,
 * parses the contents as JSON, and returns a cJSON object.
 *
 * @param file_name The name of the file to read (e.g., "config.json").
 * @return cJSON* A pointer to the parsed cJSON object, or NULL on error.
 *
 * @note The caller is responsible for freeing the returned cJSON object using cJSON_Delete().
 */
cJSON *little_file_system_read_file_to_json(const char *file_name);

/**
 * @brief Serialize a cJSON object and write it to a file in the LittleFS configuration directory.
 *
 * This function serializes the given cJSON object into a compact JSON string and writes it
 * to a file located at CONFIG_PATH/file_path. The file will be created or overwritten.
 *
 * @param json_obj Pointer to a valid cJSON object to write.
 * @param file_path Name of the file to write (relative to CONFIG_PATH).
 * @return true on success, false on failure.
 *
 * @note The function internally deletes the provided cJSON object using cJSON_Delete().
 */
bool little_file_system_write_json_to_file(cJSON *json_obj, const char *file_path);

/**
 * @brief Reads a JSON file and parses it into a cJSON object.
 *
 * This function reads the contents of a file located in ROOT_PATH,
 * parses the contents as JSON, and returns a cJSON object.
 *
 * @param file_name The name of the file to read (e.g., "config.json").
 * @return cJSON* A pointer to the parsed cJSON object, or NULL on error.
 *
 * @note The caller is responsible for freeing the returned cJSON object using cJSON_Delete().
 */
cJSON *little_file_system_read_root_file_to_json(const char *file_name);

/**
 * @brief Serialize a cJSON object and write it to a file in the LittleFS configuration directory.
 *
 * This function serializes the given cJSON object into a compact JSON string and writes it
 * to a file located at ROOT_PATH/file_path. The file will be created or overwritten.
 *
 * @param json_obj Pointer to a valid cJSON object to write.
 * @param file_path Name of the file to write (relative to ROOT_PATH).
 * @return true on success, false on failure.
 *
 * @note The function internally deletes the provided cJSON object using cJSON_Delete().
 */
bool little_file_system_write_json_to_root_file(cJSON *json_obj, const char *file_path);

#endif // OPEN_NEXTION_COMPONENTS_LFS_INCLUDE_LITTLE_FILE_SYSTEM_H

#ifdef __cplusplus
}
#endif
