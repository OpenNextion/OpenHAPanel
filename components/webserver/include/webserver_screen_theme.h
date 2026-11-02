#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_SCREEN_THEME_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_SCREEN_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "webserver_method_api.h"

/** Handle Device.GetScreenTheme for an authenticated WebSocket client. */
esp_err_t method_api_handle_get_screen_theme(cJSON *data,
                                             async_resp_arg *now_resp_arg);

/** Handle Device.SetScreenTheme for an authenticated WebSocket client. */
esp_err_t method_api_handle_set_screen_theme(cJSON *data,
                                             async_resp_arg *now_resp_arg);

/** Broadcast the selected screen theme to authenticated clients. */
esp_err_t webserver_screen_theme_notify(void);

/** Send the selected screen theme to one authenticated client. */
esp_err_t webserver_screen_theme_notify_to_client(int socket_fd);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_SCREEN_THEME_H
