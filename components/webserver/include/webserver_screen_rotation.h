#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_SCREEN_ROTATION_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_SCREEN_ROTATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "webserver_method_api.h"

/** Handle Device.SetScreenRotation for an authenticated WebSocket client. */
esp_err_t method_api_handle_set_screen_rotation(cJSON *data,
                                                async_resp_arg *now_resp_arg);

/** Broadcast the currently applied rotation to authenticated clients. */
esp_err_t webserver_screen_rotation_notify(void);
esp_err_t webserver_screen_rotation_notify_to_client(int socket_fd);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_SCREEN_ROTATION_H
