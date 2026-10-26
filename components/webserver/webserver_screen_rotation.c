#include "webserver_screen_rotation.h"

#include <stdint.h>

#include "bsp_lcd.h"
#include "cJSON.h"
#include "esp_log.h"
#include "screen_rotation.h"
#include "webserver_api.h"

static const char *TAG = "webserver_screen_rotation";

static const char *webserver_screen_rotation_orientation_to_string(
    bsp_lcd_orientation_t orientation)
{
    return orientation == BSP_LCD_ORIENTATION_LANDSCAPE
               ? "landscape"
               : "portrait";
}

static esp_err_t webserver_screen_rotation_create_data(cJSON **data_out)
{
    if (!data_out) {
        return ESP_ERR_INVALID_ARG;
    }
    *data_out = NULL;

    int32_t width = 0;
    int32_t height = 0;
    esp_err_t err = bsp_lcd_get_logical_resolution(&width, &height);
    if (err != ESP_OK) {
        return err;
    }

    bsp_lcd_rotation_t rotation = screen_rotation_get();
    bsp_lcd_orientation_t orientation = bsp_lcd_get_orientation();
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddNumberToObject(data, "rotation", (int)rotation) ||
        !cJSON_AddStringToObject(
            data,
            "orientation",
            webserver_screen_rotation_orientation_to_string(orientation)) ||
        !cJSON_AddNumberToObject(data, "width", width) ||
        !cJSON_AddNumberToObject(data, "height", height)) {
        cJSON_Delete(data);
        return ESP_ERR_NO_MEM;
    }

    *data_out = data;
    return ESP_OK;
}

static esp_err_t webserver_screen_rotation_send_error(
    async_resp_arg *now_resp_arg,
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
        WEB_API_METHOD_SET_SCREEN_ROTATION_RSP,
        now_resp_arg->seq,
        WEB_API_ERROR_FAIL);
    return send_err == ESP_OK ? operation_error : send_err;
}

esp_err_t method_api_handle_set_screen_rotation(cJSON *data,
                                                async_resp_arg *now_resp_arg)
{
    if (!now_resp_arg) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *rotation_item = data && cJSON_IsObject(data)
                               ? cJSON_GetObjectItemCaseSensitive(data, "rotation")
                               : NULL;
    if (!rotation_item || !cJSON_IsNumber(rotation_item) ||
        (rotation_item->valuedouble != BSP_LCD_ROTATION_0 &&
         rotation_item->valuedouble != BSP_LCD_ROTATION_90)) {
        ESP_LOGW(TAG, "Invalid rotation parameter; expected 0 or 90");
        return webserver_screen_rotation_send_error(
            now_resp_arg,
            ESP_ERR_INVALID_ARG,
            "rotation must be 0 or 90");
    }

    bsp_lcd_rotation_t rotation =
        (bsp_lcd_rotation_t)rotation_item->valueint;
    esp_err_t err = screen_rotation_set(rotation);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to set screen rotation to %d: %s",
                 (int)rotation,
                 esp_err_to_name(err));
        return webserver_screen_rotation_send_error(
            now_resp_arg,
            err,
            "failed to apply screen rotation");
    }

    cJSON *response = NULL;
    err = webserver_screen_rotation_create_data(&response);
    if (err != ESP_OK) {
        return webserver_screen_rotation_send_error(
            now_resp_arg,
            err,
            "screen rotation state is unavailable");
    }

    return webserver_data_code_response_to_current_client(
        response,
        WEB_API_METHOD_SET_SCREEN_ROTATION_RSP,
        now_resp_arg->seq,
        WEB_API_ERROR_OK);
}

esp_err_t webserver_screen_rotation_notify(void)
{
    cJSON *data = NULL;
    esp_err_t err = webserver_screen_rotation_create_data(&data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Cannot build screen rotation notification: %s",
                 esp_err_to_name(err));
        return err;
    }

    return webserver_data_response_to_authenticated_clients(
        data,
        WEB_API_METHOD_SCREEN_ROTATION_NOTIFY,
        0);
}

esp_err_t webserver_screen_rotation_notify_to_client(int socket_fd)
{
    cJSON *data = NULL;
    esp_err_t err = webserver_screen_rotation_create_data(&data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Cannot build screen rotation notification: %s",
                 esp_err_to_name(err));
        return err;
    }

    return webserver_data_response_to_special_client(
        data, WEB_API_METHOD_SCREEN_ROTATION_NOTIFY, 0, socket_fd);
}
