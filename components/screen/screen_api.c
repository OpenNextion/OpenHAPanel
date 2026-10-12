#include "screen_api.h"

#include "esp_log.h"
#include "screen.h"
#include "screen_validate.h"
#include "webserver_api.h"

static const char *TAG = "screen_api";

esp_err_t method_api_handle_set_screen_config(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t ret = ESP_FAIL;

    ret = screen_validate_config(data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid screen config");
        goto send_response;
    }

    ret = screen_save_config(data);

send_response:
    webserver_code_response_to_current_client(ret, WEB_API_METHOD_SET_SCREEN_CONFIG_RSP, now_resp_arg->seq);
    return ret;
}

esp_err_t screen_config_notify(void)
{
    cJSON *data = screen_load_config();
    if (!data) {
        ESP_LOGE(TAG, "Failed to load screen config for notify");
        return ESP_FAIL;
    }

    return webserver_data_response_to_current_client(data, WEB_API_METHOD_SCREEN_CONFIG_NOTIFY, 0);
}

esp_err_t screen_config_notify_authenticated_clients(void)
{
    cJSON *data = screen_load_config();
    if (!data) {
        ESP_LOGE(TAG, "Failed to load screen config for authenticated client notification");
        return ESP_FAIL;
    }

    return webserver_data_response_to_authenticated_clients(
        data,
        WEB_API_METHOD_SCREEN_CONFIG_NOTIFY,
        0);
}
