#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "webserver_method_api.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t method_api_handle_resume_session(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_logout(cJSON *data, async_resp_arg *now_resp_arg);
esp_err_t method_api_handle_revoke_all_sessions(cJSON *data, async_resp_arg *now_resp_arg);

#ifdef __cplusplus
}
#endif
