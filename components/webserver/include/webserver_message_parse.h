#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_MESSAGE_PARSE_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_MESSAGE_PARSE_H

#include "webserver.h"

#define ESP32_REBOOT "esp32"

/**
 * @brief Handle incoming webserver data messages.
 *
 * This function processes a raw JSON payload received by the webserver.
 * It performs the following steps:
 * - Validates the payload and logs the message content (truncating if too long).
 * - Parses the JSON payload and extracts the 'data', 'seq', and 'method' fields.
 * - Retrieves the async response argument and assigns the sequence number.
 * - Dispatches the request to the appropriate API handler based on the 'method' field.
 * - Cleans up allocated resources before returning.
 *
 * @param payload  Pointer to the incoming JSON message buffer. Ownership is transferred to this function.
 * @param len      Length of the payload in bytes.
 */
void webserver_data_message_handler(uint8_t *payload, size_t len);

/**
 * @brief Decrypt and parse password from incoming cJSON data.
 *
 * This function extracts the encrypted password content (`content`) and IV (`iv`) from the provided cJSON object,
 * decodes them from base64, and decrypts the password using AES-CBC with a shared key derived from the client's nonce.
 * The decrypted password is then parsed from the decrypted JSON string and stored in the provided output buffer.
 *
 * @param root        The root cJSON object containing "iv" and "content" fields.
 * @param password        Output buffer to store the decrypted password (must be at least 65 bytes).
 * @param password_len    Output length of the decrypted password.
 * @param resp_arg    Asynchronous response argument containing the client's file descriptor.
 *
 * @return ESP_OK on success, or an appropriate ESP_FAIL on error.
 */
esp_err_t parse_set_password(cJSON *root, char *password, size_t *password_len, int sock_fd);

/**
 * @brief Web API handler to set a new password via WebSocket.
 *
 * This function handles the decrypted password setting request from the client. The password is saved
 * to system configuration, used to update AP settings if needed, and a WebSocket response is returned
 * with the result.
 *
 * Special logic is included if this request is part of a "forgot password" workflow.
 *
 * @param data             JSON payload from WebSocket (already parsed).
 * @param now_resp_arg     Async response context, including socket FD and sequence ID.
 *
 * @return ESP_OK on success, or error code from processing steps.
 */
esp_err_t method_api_handle_set_password(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Handle the "set login state" API request.
 *
 * Verifies the provided password correctness from the incoming data and responds to the client
 * with the verification result.
 *
 * @param data           Pointer to the cJSON object containing the login request data.
 * @param now_resp_arg   Pointer to the async response argument struct, used for responding to the client.
 *
 * @return
 *      - ESP_OK on success.
 *      - Other esp_err_t error codes on failure.
 */
esp_err_t method_api_handle_set_login_state(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Handle the reset device API call.
 *
 * Initiates a factory reset sequence with visual feedback and system event posting.
 *
 * @param data          JSON input data.
 * @param now_resp_arg  Async response argument for client response.
 *
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t method_api_handle_reset_device(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief API handler for setting the IP whitelist.
 *
 * Updates the IP whitelist configuration based on the received JSON data.
 *
 * @param data          JSON input containing IP whitelist information.
 * @param now_resp_arg  Async response context for client response.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t method_api_handle_set_ip_white_list(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief API handler to set the device theme mode.
 *
 * Parses and updates the device theme mode based on the input JSON data.
 *
 * @param data          JSON input containing theme mode information.
 * @param now_resp_arg  Async response context for client response.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t method_api_handle_set_theme_mode(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief API handler to set the device language.
 *
 * Parses the language from JSON input and updates the device information accordingly.
 *
 * @param data          JSON input containing the language setting.
 * @param now_resp_arg  Async response context for client response.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t method_api_handle_set_language(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief API handler to set the device brightness.
 *
 * Parses the brightness from JSON input and updates the device information accordingly.
 *
 * @param data          JSON input containing the brightness setting.
 * @param now_resp_arg  Async response context for client response.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t method_api_handle_set_brightness(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief API handler to set the night mode.
 *
 * Parses the night mode from JSON input and updates the device information accordingly.
 *
 * @param data          JSON input containing the night mode setting.
 * @param now_resp_arg  Async response context for client response.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t method_api_handle_set_night_mode(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief API handler to set the device idle time.
 *
 * Parses the idle time from JSON input and updates the device information accordingly.
 *
 * @param data          JSON input containing the idle time setting.
 * @param now_resp_arg  Async response context for client response.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t method_api_handle_set_screen_lock(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Reset the screen lock timer.
 *
 * This function is called when the screen is interacted with to reset the timer.
 * The timer is used to lock the screen after a period of inactivity.
 *
 */
void screen_lock_reset_timer(void);

/**
 * @brief API handler to set the device product name.
 *
 * Parses the product name from JSON input and updates the device information accordingly.
 *
 * @param data          JSON input containing the product name.
 * @param now_resp_arg  Async response context for client response.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t method_api_handle_set_name(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief API handler to reboot device or modules based on the target parameter.
 *
 * Supports rebooting the ESP32, wireless_controller module, or both.
 *
 * @param data          JSON payload containing the reboot target.
 * @param now_resp_arg  Async response argument for sending client response.
 *
 * @return esp_err_t    Result of the reboot command handling.
 */
esp_err_t method_api_handle_reboot_device(cJSON *data, async_resp_arg *now_resp_arg);

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_MESSAGE_PARSE_H

#ifdef __cplusplus
}
#endif
