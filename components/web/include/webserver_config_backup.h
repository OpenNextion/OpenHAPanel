#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_BACKUP_H
#define OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_BACKUP_H

#include "esp_event.h"
#include "webserver_method_api.h"

/**
 * @brief Event base declaration for configuration-related events.
 *
 * This base is used for all events related to configuration management,
 * such as backup uploads and status reporting.
 */
ESP_EVENT_DECLARE_BASE(CONFIG_EVENT);

/**
 * @brief Error code indicating that a requested backup file does not exist on the device.
 *
 * This is typically returned when the upload or restoration process cannot
 * find a valid backup file in the expected location or format.
 */
#define LFS_ERR_NO_DEVICE_BACKUP_FILE (300)

/**
 * @brief Configuration event identifiers used with CONFIG_EVENT base.
 */
enum
{
    /**
     * @brief Event triggered when a backup configuration is successfully uploaded.
     */
    CONFIG_EVENT_UPLOAD = 0,

    /**
     * @brief Event triggered to report current configuration status.
     */
    CONFIG_EVENT_STATUS_REPORT,
};

/**
 * @brief Retrieve the encrypted system configuration as a binary buffer.
 *
 * This function collects all configuration files, packs them into a JSON object,
 * serializes it, and encrypts the resulting string using AES-128-CBC.
 *
 * The caller is responsible for freeing the returned buffer.
 *
 * @param[out] out_size Pointer to store the length of the encrypted data.
 *
 * @return Pointer to the encrypted buffer on success, or NULL on failure.
 */
uint8_t *webserver_config_backup_get_encrypted_sys_config(int32_t *out_size);

/**
 * @brief Handle system configuration backup download request via WebSocket API.
 *
 * This function retrieves the current system configuration, encrypts it, encodes it
 * in Base64 format, and sends it to the client as a WebSocket response.
 *
 * @param[in] data           Unused JSON input (can be NULL or empty).
 * @param[in] now_resp_arg   Pointer to the response argument structure for tracking session and sequence.
 *
 * @return
 *        - ESP_OK on success.
 *        - ESP_FAIL or ESP_ERR_NO_MEM on failure.
 */
esp_err_t method_api_handle_download_backup(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Handle upload of encrypted system configuration backup via WebSocket API.
 *
 * This function decodes a Base64-encoded, AES-128-CBC-encrypted configuration payload
 * received from the client. It validates, decrypts, parses, and stores the configuration
 * if valid. A response with the operation result is sent back to the client.
 *
 * @param[in] data           A cJSON object containing the "content" field with encoded data.
 * @param[in] now_resp_arg   Pointer to the current async response arguments, used for responding.
 *
 * @return
 *        - ESP_OK on success.
 *        - ESP_FAIL on failure.
 */
esp_err_t method_api_handle_upload_backup(cJSON *data, async_resp_arg *now_resp_arg);

#endif // OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_BACKUP_H

#ifdef __cplusplus
}
#endif
