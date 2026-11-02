#include "webserver_screen_theme.h"

#include <stdbool.h>

#include "cJSON.h"
#include "esp_log.h"
#include "screen_theme.h"
#include "webserver_api.h"

static const char *TAG = "webserver_screen_theme";

static esp_err_t webserver_screen_theme_create_data(bool include_changed,
                                                    bool changed,
                                                    cJSON **data_out)
{
    if (!data_out) {
        return ESP_ERR_INVALID_ARG;
    }
    *data_out = NULL;

    const char *theme = screen_theme_mode_to_string(screen_theme_get());
    if (!theme) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(data, "theme", theme) ||
        (include_changed &&
         !cJSON_AddBoolToObject(data, "changed", changed))) {
        cJSON_Delete(data);
        return ESP_ERR_NO_MEM;
    }

    *data_out = data;
    return ESP_OK;
}

static esp_err_t webserver_screen_theme_send_error(
    async_resp_arg *now_resp_arg,
    char *response_method,
    esp_err_t operation_error,
    const char *message)
{
    cJSON *data = cJSON_CreateObject();
    if (data) {
        if (!cJSON_AddStringToObject(data, "message", message) ||
            !cJSON_AddStringToObject(data,
                                     "reason",
                                     esp_err_to_name(operation_error))) {
            cJSON_Delete(data);
            data = NULL;
        }
    }

    esp_err_t send_err = webserver_data_code_response_to_current_client(
        data,
        response_method,
        now_resp_arg->seq,
        WEB_API_ERROR_FAIL);
    return send_err == ESP_OK ? operation_error : send_err;
}

esp_err_t method_api_handle_get_screen_theme(cJSON *data,
                                             async_resp_arg *now_resp_arg)
{
    (void)data;
    if (!now_resp_arg) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *response = NULL;
    esp_err_t err = webserver_screen_theme_create_data(false,
                                                       false,
                                                       &response);
    if (err != ESP_OK) {
        return webserver_screen_theme_send_error(
            now_resp_arg,
            WEB_API_METHOD_GET_SCREEN_THEME_RSP,
            err,
            "screen theme state is unavailable");
    }

    return webserver_data_code_response_to_current_client(
        response,
        WEB_API_METHOD_GET_SCREEN_THEME_RSP,
        now_resp_arg->seq,
        WEB_API_ERROR_OK);
}

esp_err_t method_api_handle_set_screen_theme(cJSON *data,
                                             async_resp_arg *now_resp_arg)
{
    if (!now_resp_arg) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *theme_item = data && cJSON_IsObject(data)
                            ? cJSON_GetObjectItemCaseSensitive(data, "theme")
                            : NULL;
    screen_theme_mode_t theme = SCREEN_THEME_DARK;
    if (!cJSON_IsString(theme_item) || !theme_item->valuestring ||
        screen_theme_mode_from_string(theme_item->valuestring, &theme) !=
            ESP_OK) {
        ESP_LOGW(TAG, "Invalid theme parameter; expected dark or light");
        return webserver_screen_theme_send_error(
            now_resp_arg,
            WEB_API_METHOD_SET_SCREEN_THEME_RSP,
            ESP_ERR_INVALID_ARG,
            "theme must be dark or light");
    }

    bool changed = false;
    esp_err_t err = screen_theme_set_with_changed(theme, &changed);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to set screen theme to %s: %s",
                 theme_item->valuestring,
                 esp_err_to_name(err));
        return webserver_screen_theme_send_error(
            now_resp_arg,
            WEB_API_METHOD_SET_SCREEN_THEME_RSP,
            err,
            "failed to apply screen theme");
    }

    cJSON *response = NULL;
    err = webserver_screen_theme_create_data(true, changed, &response);
    if (err != ESP_OK) {
        return webserver_screen_theme_send_error(
            now_resp_arg,
            WEB_API_METHOD_SET_SCREEN_THEME_RSP,
            err,
            "screen theme state is unavailable");
    }

    esp_err_t response_err = webserver_data_code_response_to_current_client(
        response,
        WEB_API_METHOD_SET_SCREEN_THEME_RSP,
        now_resp_arg->seq,
        WEB_API_ERROR_OK);
    if (response_err != ESP_OK || !changed) {
        return response_err;
    }

    esp_err_t notify_err = webserver_screen_theme_notify();
    if (notify_err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to broadcast screen theme notification: %s",
                 esp_err_to_name(notify_err));
    }
    return notify_err;
}

esp_err_t webserver_screen_theme_notify(void)
{
    cJSON *data = NULL;
    esp_err_t err = webserver_screen_theme_create_data(false, false, &data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Cannot build screen theme notification: %s",
                 esp_err_to_name(err));
        return err;
    }

    return webserver_data_response_to_authenticated_clients(
        data,
        WEB_API_METHOD_SCREEN_THEME_NOTIFY,
        0);
}

esp_err_t webserver_screen_theme_notify_to_client(int socket_fd)
{
    cJSON *data = NULL;
    esp_err_t err = webserver_screen_theme_create_data(false, false, &data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Cannot build screen theme notification: %s",
                 esp_err_to_name(err));
        return err;
    }

    return webserver_data_response_to_special_client(
        data,
        WEB_API_METHOD_SCREEN_THEME_NOTIFY,
        0,
        socket_fd);
}
