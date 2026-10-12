#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_API_H
#define OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_API_H

#include "webserver_method_api.h"

/**
 * @brief Handle Device.SetScreenConfig WebSocket requests.
 *
 * Validates the request data, saves it through the screen component, and replies
 * with Device.SetScreenConfigRsp.
 *
 * @param data Request data object containing the screens array.
 * @param now_resp_arg Current websocket response context.
 * @return ESP_OK on success, ESP_FAIL (-1) or another esp_err_t on failure.
 */
esp_err_t method_api_handle_set_screen_config(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Push Device.ScreenConfigNotify to the current authenticated client.
 *
 * The notification payload is loaded from screen.json.
 *
 * @return ESP_OK on success, ESP_FAIL (-1) on load/send failure.
 */
esp_err_t screen_config_notify(void);

/**
 * @brief Broadcast Device.ScreenConfigNotify to all authenticated clients.
 *
 * The notification payload is loaded from screen.json.
 *
 * @return ESP_OK on success, or an error when the config cannot be loaded or sent.
 */
esp_err_t screen_config_notify_authenticated_clients(void);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_SCREEN_INCLUDE_SCREEN_API_H
