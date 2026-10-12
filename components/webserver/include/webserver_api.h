#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_API_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_API_H

#include "webserver_method_api.h"

/**
 * @brief Calculates the SHA-256 hash of the input data.
 *
 * Computes the SHA-256 digest of the input buffer and writes the raw binary
 * hash (32 bytes) to the provided output buffer.
 *
 * @param in     Pointer to input data buffer.
 * @param len    Length of the input buffer in bytes.
 * @param out    Pointer to a buffer (at least 32 bytes) to store the hash output.
 *
 * @return
 *         - ESP_OK on success
 *         - ESP_FAIL on error (e.g., invalid parameters or internal failure)
 */
esp_err_t sha256_get(const uint8_t *in, uint32_t len, char *out);

/**
 * @brief Retrieve the current asynchronous websocket response argument.
 *
 * @return Pointer to the current async_resp_arg object.
 *
 * @note The returned pointer references internal static data.
 *       Do not free or modify this pointer directly without caution.
 */
async_resp_arg *websocket_get_async_rsp_arg(void);

/**
 * @brief Update the current asynchronous websocket response argument.
 *
 * @param arg Pointer to an async_resp_arg structure containing the new response data.
 *
 * @note The data pointed by arg is copied into internal storage.
 */
void websocket_set_async_rsp_arg(async_resp_arg *arg);

/**
 * @brief Determines whether the client IP address belongs to the AP (Access Point) subnet.
 *
 * This function retrieves the IP address associated with the given socket file descriptor
 * and compares it against the IP and subnet mask of the default Wi-Fi Access Point (WIFI_AP_DEF).
 *
 * @note This function supports both IPv4 and IPv6 configurations depending on the build-time settings.
 *
 * @param socket_fd The file descriptor of the socket connected to the client.
 * @param ip_addr A pointer to an in_addr_t variable where the client's IP address will be stored (only valid in IPv4).
 *
 * @return true if the client IP address is within the AP subnet range; false otherwise.
 */
bool webserver_client_ip_is_ap_subnet(int socket_fd, in_addr_t *ip_addr);

/**
 * @brief Retrieve the nonce associated with a connected client.
 *
 * This function looks up a client node by its socket file descriptor and copies the stored nonce value.
 *
 * @param sock_fd   The socket file descriptor of the client connection.
 * @param nonce_out A pointer to a character buffer where the nonce will be copied
 *                  (must be at least WEB_AUTH_NONCE_HEX_LEN + 1 bytes).
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if input parameters are invalid
 *      - ESP_FAIL if the client is not found in the active connection list
 */
esp_err_t webserver_get_client_nonce(int sock_fd, char *nonce_out);

/**
 * @brief Update the nonce value associated with a connected client.
 *
 * This function updates the stored nonce for a specific client, identified by its socket file descriptor.
 * The nonce is typically a unique session identifier (e.g., UUID).
 *
 * @param sock_fd   The socket file descriptor identifying the client.
 * @param nonce_in  A pointer to the new nonce string to be stored.
 *
 * @note This function performs parameter validation and logs appropriate error messages.
 *       It does not return a value; failures are logged internally.
 */
void webserver_update_client_nonce(int sock_fd, char *nonce_in);

/**
 * @brief Check if the number of connected WebSocket webserver_node_clients has reached the configured maximum.
 *
 * This function compares the current number of active WebSocket webserver_node_clients against the
 * predefined limit (`MAX_WEBSOCKET_CLIENT`).
 *
 * @return true if the client count exceeds the limit; false otherwise.
 */
bool websocket_client_is_limit(void);

/**
 * @brief Broadcast a WebSocket message to all connected webserver_node_clients.
 *
 * This function sends a WebSocket message to every client currently connected to the server.
 * It ensures thread safety by locking the client list with a mutex during iteration.
 *
 * @param msg  Pointer to the message payload.
 * @param len  Length of the message in bytes.
 * @param type WebSocket frame type (e.g., HTTPD_WS_TYPE_TEXT, HTTPD_WS_TYPE_BINARY).
 *
 * @return
 *      - ESP_OK if all messages were sent successfully.
 *      - ESP_FAIL if the mutex could not be acquired or if any send failed.
 *      - ESP_ERR_INVALID_ARG if the message is NULL or length is 0.
 */
esp_err_t webserver_send_message_to_all_client(char *msg, size_t len);

/** Send a WebSocket message only to authenticated clients. */
esp_err_t webserver_send_message_to_authenticated_clients(char *msg, size_t len);

/**
 * @brief Send a WebSocket message to the currently connected client in the async context.
 *
 * This function is used within WebSocket asynchronous callbacks to respond to the specific
 * client that triggered the callback. It ensures the message is sent in a thread-safe
 * manner using a semaphore.
 *
 * @param msg   Pointer to the message payload.
 * @param len   Length of the message in bytes.
 * @param type  WebSocket frame type (e.g., `HTTPD_WS_TYPE_TEXT`, `HTTPD_WS_TYPE_BINARY`).
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_ERR_INVALID_ARG if parameters are invalid.
 *      - ESP_FAIL if the async context or sending mechanism fails.
 */
esp_err_t webserver_send_message_to_current_client(char *msg, size_t len);

/**
 * @brief Send a WebSocket message to a specific client by socket descriptor.
 *
 * This function allows server-side code to target a particular WebSocket client directly
 * by providing its socket file descriptor.
 *
 * @param msg        Pointer to the message payload.
 * @param len        Length of the message in bytes.
 * @param socket_fd  File descriptor of the client socket.
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_ERR_INVALID_ARG if input is invalid.
 *      - ESP_FAIL if sending the message fails.
 */
esp_err_t webserver_send_message_to_special_client(char *msg, size_t len, int socket_fd);

/**
 * @brief Send a WebSocket message not log to a specified client by socket file descriptor.
 *
 * This function sends the provided WebSocket message to a client identified by its socket FD.
 * It ensures thread-safe transmission using a semaphore-based send helper.
 *
 * @param msg        Pointer to the message payload.
 * @param len        Length of the message in bytes.
 * @param type       WebSocket frame type (e.g., `HTTPD_WS_TYPE_TEXT`, `HTTPD_WS_TYPE_BINARY`).
 * @param socket_fd  Socket file descriptor of the target WebSocket client.
 *
 * @return
 *      - ESP_OK on successful transmission.
 *      - ESP_ERR_INVALID_ARG if input parameters are invalid.
 *      - ESP_FAIL if sending fails.
 *
 * @note Caller must ensure that the socket_fd refers to a valid connected WebSocket client.
 */
esp_err_t webserver_send_message_to_special_client_not_log(char *msg, size_t len, int socket_fd);

/**
 * @brief Send a JSON data/error response to one client without logging its payload.
 *
 * This variant is intended for responses containing credentials or OAuth state.
 * Ownership of data is transferred to this function.
 */
esp_err_t webserver_data_code_response_to_special_client_not_log(cJSON *data,
                                                                  char *method,
                                                                  double seq,
                                                                  int error_code,
                                                                  int socket_fd);

/** Send a JSON notification to one client without logging its payload. */
esp_err_t webserver_data_response_to_special_client_not_log(cJSON *data,
                                                             char *method,
                                                             double seq,
                                                             int socket_fd);

/**
 * @brief Close all non-AP webserver_node_clients except the one with the given socket descriptor.
 *
 * This function closes all WebSocket connections from webserver_node_clients connected via Wi-Fi Station or
 * Ethernet, preserving only the one identified by `current_socket_fd` and those on AP.
 *
 * @param current_socket_fd  File descriptor of the current client to remain connected.
 */
void webserver_close_other_wifi_and_eth_clients(int current_socket_fd);

/**
 * @brief Close the WebSocket client session with the specified socket descriptor.
 *
 * This function is typically used to disconnect a specific client from the WebSocket server,
 * identified by its socket file descriptor.
 *
 * @param current_socket_fd  File descriptor of the client to close.
 */
void webserver_close_current_client(int current_socket_fd);

/**
 * @brief Send a JSON-formatted error response to the current WebSocket client.
 *
 * This function sends a response containing the specified error code, method name,
 * and sequence number in JSON format to the active WebSocket session.
 *
 * @param error_code  Integer error code to send.
 * @param method      String representing the method name.
 * @param seq         Request sequence number (usually from the client).
 *
 * @return
 *      - ESP_OK if the message was successfully sent.
 *      - ESP_FAIL on failure (e.g., memory allocation or WebSocket error).
 */
esp_err_t webserver_code_response_to_current_client(int error_code, char *method, double seq);

/**
 * @brief Send a structured error response to a specific WebSocket client.
 *
 * Builds a JSON message with method name, sequence number, and error code,
 * and sends it as a text frame to the client associated with the given socket FD.
 *
 * @param error_code   Error code to include in the JSON response.
 * @param method       String representing the method name (null-terminated).
 * @param seq          Sequence number to identify the request/response.
 * @param socket_fd    Target client’s socket file descriptor.
 *
 * @return
 *      - ESP_OK on successful message dispatch.
 *      - ESP_FAIL on failure (e.g., memory error, transmission failure).
 */
esp_err_t webserver_code_response_to_special_client(int error_code, char *method, double seq, int socket_fd);

/**
 * @brief Send a JSON error response to all connected WebSocket webserver_node_clients.
 *
 * Constructs and broadcasts a JSON-formatted error response including the
 * method name, sequence number, and error code to every active client.
 *
 * @param error_code   The application-defined error code.
 * @param method       The method name string associated with the original request.
 * @param seq          The request sequence number.
 *
 * @return
 *      - ESP_OK if messages are successfully dispatched.
 *      - ESP_FAIL if there is an internal failure (e.g., memory or socket issues).
 */
esp_err_t webserver_code_response_to_all_client(int error_code, char *method, double seq);

/**
 * @brief Send a structured JSON data response to the current WebSocket client.
 *
 * Constructs a WebSocket JSON message containing a method name, a timestamp (in milliseconds),
 * an optional sequence number, and a data payload.
 *
 * @param data        Pointer to the cJSON object representing the payload.
 * @param method      The method name to include in the response.
 * @param seq         Sequence number; set to 0 if not required.
 *
 * @return
 *      - ESP_OK if the message is successfully sent.
 *      - ESP_FAIL on memory allocation or send error.
 */
esp_err_t webserver_data_response_to_current_client(cJSON *data, char *method, double seq);

/**
 * @brief Send a structured JSON data response to the special WebSocket client.
 *
 * Constructs a WebSocket JSON message containing a method name, a timestamp (in milliseconds),
 * an optional sequence number, and a data payload.
 *
 * @param data        Pointer to the cJSON object representing the payload.
 * @param method      The method name to include in the response.
 * @param seq         Sequence number; set to 0 if not required.
 *
 * @return
 *      - ESP_OK if the message is successfully sent.
 *      - ESP_FAIL on memory allocation or send error.
 */
esp_err_t webserver_data_response_to_special_client(cJSON *data, char *method, double seq, int socket_fd);

/**
 * @brief Send a JSON data response to the current WebSocket client.
 *
 * This function constructs a JSON response containing a method name, timestamp,
 * optional sequence number, and a `data` object. It sends the resulting JSON message
 * as a WebSocket frame to the currently active client associated with the ongoing session.
 *
 * JSON Format Example:
 * {
 *   "method": "<method_name>",
 *   "error_code": <error_code>,
 *   "data": {...},          // The actual payload, passed in via `data`
 *   "seq": <optional_sequence_number>
 * }
 *
 * @param data        A pointer to a cJSON object to include under the "data" key.
 *                    Ownership is transferred and will be deleted by cJSON after sending.
 * @param method      Null-terminated string indicating the API method name.
 * @param seq         Optional sequence number (set to 0 if unused).
 * @param error_code  Error_code
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_FAIL if memory allocation fails or message sending fails.
 *
 * @note This function transfers the ownership of `data` to the final JSON response.
 *       Do not use `data` after calling this function.
 */
esp_err_t webserver_data_code_response_to_current_client(cJSON *data, char *method, double seq, int error_code);

/**
 * @brief Broadcast a JSON data message to all connected WebSocket webserver_node_clients.
 *
 * @param data     Pointer to cJSON data object to send.
 * @param method   The method name string for the message.
 * @param seq      Optional sequence number; set to 0 if unused.
 *
 * @return
 *      - ESP_OK if the message is successfully sent.
 *      - ESP_FAIL on memory or send failure.
 */
esp_err_t webserver_data_response_to_all_client(cJSON *data, char *method, double seq);

/** Broadcast a JSON notification only to authenticated clients. */
esp_err_t webserver_data_response_to_authenticated_clients(cJSON *data, char *method, double seq);

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_API_H
#ifdef __cplusplus
}
#endif
