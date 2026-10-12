#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEB_WEBSERVER_CONFIG_FORGET_PASSWORD_H
#define OPEN_NEXTION_COMPONENTS_WEB_WEBSERVER_CONFIG_FORGET_PASSWORD_H

#include <stdbool.h>

#include "screen_state.h"
#include "webserver_method_api.h"

#define SET_FORGOT_PASSWORD_WAITING "waiting" /* Indicates waiting for the user to double-click the key */

#define SET_FORGOT_PASSWORD_READY "ready" /* indicates that the user has double-clicked the key */

#define SET_FORGOT_PASSWORD_DONE "done" /* the forgotten password process ends */

typedef enum {
    WEBSERVER_FORGOT_PASSWORD_STATE_DONE = 0,
    WEBSERVER_FORGOT_PASSWORD_STATE_WAITING,
    WEBSERVER_FORGOT_PASSWORD_STATE_READY,
} webserver_forgot_password_state_t;

void check_and_clear_forget_password_socket(int fd);

/**
 * @brief Cancels the password reset timer and notifies the WebServer client.
 *
 * If the password reset process was started, this function:
 * - Sends a notification that the process is complete.
 * - Deletes the active timeout timer (if any).
 * - Resets the start flag.
 * - Closes the WebServer connection associated with the provided socket file descriptor.
 *
 * @param[in] set_password_fd The socket file descriptor used to close the client connection.
 */
void webserver_config_forget_password_cancel_timer(int set_password_fd);

/**
 * @brief Notifies the waiting task that a double-click event has occurred.
 *
 * If the password reset waiting task is running, this function:
 * - Sets an RGB LED to the password reset indication.
 * - Sends a notification to the task indicating a double-click event.
 */
void webserver_config_forget_password_double_click(void);

bool webserver_config_forget_password_is_authorized(int socket_fd);

/** Return the current password-recovery state maintained by the device. */
webserver_forgot_password_state_t webserver_config_forget_password_get_state(void);

/** Notify the client that initiated the password-recovery flow of the current state. */
esp_err_t webserver_config_forget_password_state_notify(void);

/** Send the current password-recovery state to a client only if it initiated the flow. */
esp_err_t webserver_config_forget_password_state_notify_to_client(int socket_fd);

/** Return the full-screen page required by the active password-recovery flow. */
screen_page_t webserver_config_forget_password_get_screen_page(void);

/**
 * @brief Handle the "set forgot password" API request.
 *
 * Parses the request to control the forgot password process. Manages the state of the process,
 * including starting, waiting for confirmation, and completing the process.
 *
 * @param data           Pointer to the cJSON object containing the forgot password data.
 * @param now_resp_arg   Pointer to async response arguments used for client communication.
 *
 * @return
 *      - ESP_OK on success.
 *      - Error codes if parsing fails or invalid states are received.
 */
esp_err_t method_api_handle_set_forgot_password(cJSON *data, async_resp_arg *now_resp_arg);

#endif // OPEN_NEXTION_COMPONENTS_WEB_WEBSERVER_CONFIG_FORGET_PASSWORD_H

#ifdef __cplusplus
}
#endif
