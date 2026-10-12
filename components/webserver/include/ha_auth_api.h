#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "webserver_method_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void ha_auth_api_init(void);

esp_err_t method_api_handle_hass_start_oauth(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_hass_complete_oauth(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_hass_get_connection_info(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_hass_update_url(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_hass_disable(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_hass_activate(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_hass_remove_authorization(cJSON *data, async_resp_arg *now_resp_arg);

#ifdef __cplusplus
}
#endif
