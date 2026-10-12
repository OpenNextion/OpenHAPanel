#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_AUTH_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_AUTH_H

#include <esp_netif.h>
#include <esp_http_server.h>
#include "webserver_method_api.h"
#include "webserver_client_node.h"

/**
 * @brief Handles client login authentication and sends authentication notification.
 *
 * Determines whether a password is required for the client IP address,
 * whether the password is initialized, and whether the maximum connection limit is reached.
 * If password authentication is needed, a SHA-256-based digest challenge is sent.
 *
 * @param resp_arg  The response argument structure used to send data back to the client.
 *
 * @return
 *         - ESP_OK on success
 *         - ESP_FAIL on memory or configuration error
 */
esp_err_t webserver_login_authentication(async_resp_arg *resp_arg, bool is_ap_subnet, uint32_t ip_addr);

/**
 * @brief Verifies user password from JSON authentication data.
 *
 * This function parses authentication fields from a cJSON object,
 * and validates the response using SHA-256 based digest authentication.
 *
 * @param root Parsed cJSON object containing all required digest auth fields.
 * @param sock_fd Socket file descriptor identifying the client to update..
 * @return
 *         - ESP_OK on successful authentication
 *         - ESP_FAIL if authentication fails or on error
 */
esp_err_t verify_password_is_correct(cJSON *root, int sock_fd);

esp_err_t webserver_auth_rotate_nonce(int sock_fd, char *nonce_out, size_t nonce_out_size);

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_AUTH_H

#ifdef __cplusplus
}
#endif
