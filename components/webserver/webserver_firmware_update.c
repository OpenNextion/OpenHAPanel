#include "webserver_firmware_update.h"

#include <string.h>

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "webserver_api.h"
#include "webserver_method_api.h"

#define FIRMWARE_UPDATE_STATE_MAX_LEN 24
#define FIRMWARE_UPDATE_TARGET_VERSION_MAX_LEN 32
#define FIRMWARE_UPDATE_PROGRESS_NOTIFY_STEP 5

typedef struct {
    bool active;
    bool operation_blocked;
    webserver_firmware_update_source_t source;
    uint8_t progress;
    char state[FIRMWARE_UPDATE_STATE_MAX_LEN];
    char target_version[FIRMWARE_UPDATE_TARGET_VERSION_MAX_LEN];
} webserver_firmware_update_status_t;

static portMUX_TYPE s_status_guard = portMUX_INITIALIZER_UNLOCKED;
static webserver_firmware_update_status_t s_status = {
    .source = WEBSERVER_FIRMWARE_UPDATE_SOURCE_NONE,
    .state = "idle",
};

static const char *webserver_firmware_update_source_to_string(
    webserver_firmware_update_source_t source)
{
    switch (source) {
    case WEBSERVER_FIRMWARE_UPDATE_SOURCE_SCREEN:
        return "screen";
    case WEBSERVER_FIRMWARE_UPDATE_SOURCE_AUTOMATIC:
        return "automatic";
    case WEBSERVER_FIRMWARE_UPDATE_SOURCE_NONE:
    default:
        return "none";
    }
}

static esp_err_t webserver_firmware_update_send_status(
    const webserver_firmware_update_status_t *status)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddBoolToObject(data, "active", status->active) ||
        !cJSON_AddBoolToObject(data, "operationBlocked", status->operation_blocked) ||
        !cJSON_AddStringToObject(data,
                                "source",
                                webserver_firmware_update_source_to_string(status->source)) ||
        !cJSON_AddStringToObject(data, "state", status->state) ||
        !cJSON_AddNumberToObject(data, "progress", status->progress) ||
        !cJSON_AddStringToObject(data, "targetVersion", status->target_version)) {
        cJSON_Delete(data);
        return ESP_ERR_NO_MEM;
    }

    return webserver_data_response_to_authenticated_clients(
        data,
        WEB_API_METHOD_FIRMWARE_UPDATE_STATUS_NOTIFY,
        0);
}

esp_err_t webserver_firmware_update_status_set(
    bool active,
    bool operation_blocked,
    webserver_firmware_update_source_t source,
    const char *state,
    uint8_t progress,
    const char *target_version)
{
    if (!state || progress > 100) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *version = target_version ? target_version : "";
    bool notify = false;

    taskENTER_CRITICAL(&s_status_guard);
    bool structure_changed = s_status.active != active ||
                             s_status.operation_blocked != operation_blocked ||
                             s_status.source != source ||
                             strcmp(s_status.state, state) != 0 ||
                             strcmp(s_status.target_version, version) != 0;
    bool progress_step_changed = operation_blocked &&
        s_status.progress / FIRMWARE_UPDATE_PROGRESS_NOTIFY_STEP !=
            progress / FIRMWARE_UPDATE_PROGRESS_NOTIFY_STEP;

    s_status.active = active;
    s_status.operation_blocked = operation_blocked;
    s_status.source = source;
    s_status.progress = progress;
    strlcpy(s_status.state, state, sizeof(s_status.state));
    strlcpy(s_status.target_version, version, sizeof(s_status.target_version));
    notify = structure_changed || progress_step_changed;
    taskEXIT_CRITICAL(&s_status_guard);

    return notify ? webserver_firmware_update_status_notify() : ESP_OK;
}

esp_err_t webserver_firmware_update_status_notify(void)
{
    webserver_firmware_update_status_t snapshot;
    taskENTER_CRITICAL(&s_status_guard);
    snapshot = s_status;
    taskEXIT_CRITICAL(&s_status_guard);
    return webserver_firmware_update_send_status(&snapshot);
}

bool webserver_firmware_update_operations_blocked(void)
{
    taskENTER_CRITICAL(&s_status_guard);
    bool blocked = s_status.operation_blocked;
    taskEXIT_CRITICAL(&s_status_guard);
    return blocked;
}

bool webserver_firmware_update_method_allowed(const char *method)
{
    if (!method) {
        return false;
    }

    static const char *const allowed_methods[] = {
        WEB_API_METHOD_SYS_SET_PASSWORD,
        WEB_API_METHOD_SYS_SET_LOGIN_STATE,
        WEB_API_METHOD_SYS_RESUME_SESSION,
        WEB_API_METHOD_SYS_EXCHANGE_PROVISIONING_HANDOFF,
        WEB_API_METHOD_SYS_LOGOUT,
        WEB_API_METHOD_HASS_GET_CONNECTION_INFO,
        WEB_API_METHOD_GET_TIMEZONE,
    };

    for (size_t index = 0;
         index < sizeof(allowed_methods) / sizeof(allowed_methods[0]);
         index++) {
        if (strcmp(method, allowed_methods[index]) == 0) {
            return true;
        }
    }
    return false;
}
