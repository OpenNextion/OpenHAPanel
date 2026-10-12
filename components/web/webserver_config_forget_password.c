#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_random.h"
#include "cJSON.h"
#include "littlefs.h"
#include "mbedtls/base64.h"
#include "sys_time.h"
#include "sys_time_sync.h"
#include "sys_time_api.h"
#include "cjson_helpers.h"
#include "webserver_message_parse.h"
#include "webserver_config_forget_password.h"
#include "key.h"
#include "screen_state.h"

/**
 * @brief Timeout duration (in milliseconds) to wait for a double-click event during password reset.
 */
#define WAIT_TIMEOUT_MS (10000)

/**
 * @brief Logger tag
 */
static const char *TAG = "webserver_config_forget_password";

/**
 * @brief Handle for the software timer that tracks password reset timeout.
 *
 * This timer is started after a successful double-click and cancels the password reset
 * if no further action is taken within the allowed time.
 */
TimerHandle_t forget_password_timeout_timer_handle = NULL;

/**
 * @brief Handle for the task that waits for a double-click event to trigger password reset.
 */
TaskHandle_t forget_password_double_click_task_handle = NULL;

/**
 * @brief Current password-recovery flow state.
 *
 * This is the authoritative device-side state used by request validation,
 * screen arbitration, and WebSocket state notifications.
 */
static volatile webserver_forgot_password_state_t forget_password_state =
    WEBSERVER_FORGOT_PASSWORD_STATE_DONE;

/**
 * @brief File descriptor of the client socket involved in the password reset interaction.
 *
 * Used to close the connection after the process completes or times out.
 */
int forget_password_socket_fd = 0;
static screen_page_t forget_password_return_page = SCREEN_PAGE_NONE;

static const char *forget_password_state_to_string(webserver_forgot_password_state_t state)
{
    switch (state) {
    case WEBSERVER_FORGOT_PASSWORD_STATE_WAITING:
        return SET_FORGOT_PASSWORD_WAITING;
    case WEBSERVER_FORGOT_PASSWORD_STATE_READY:
        return SET_FORGOT_PASSWORD_READY;
    case WEBSERVER_FORGOT_PASSWORD_STATE_DONE:
    default:
        return SET_FORGOT_PASSWORD_DONE;
    }
}

static bool forget_password_is_screen_page(screen_page_t page)
{
    return page == SCREEN_PAGE_FORGOT_PASSWORD || page == SCREEN_PAGE_RESET_PASSWORD;
}

static void forget_password_request_screen(screen_page_t page)
{
    if (!screen_state_is_initialized()) {
        ESP_LOGW(TAG, "Screen state is not initialized; cannot show %s", screen_state_to_string(page));
        return;
    }

    esp_err_t err = screen_state_request_transition(page);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to request screen %s: %s", screen_state_to_string(page), esp_err_to_name(err));
    }
}

static void forget_password_capture_return_screen(void)
{
    screen_page_t current = screen_state_get_current();
    if (current != SCREEN_PAGE_NONE && !forget_password_is_screen_page(current)) {
        forget_password_return_page = current;
    }
}

static void forget_password_restore_screen(void)
{
    screen_page_t return_page = forget_password_return_page;
    forget_password_return_page = SCREEN_PAGE_NONE;
    if (return_page != SCREEN_PAGE_NONE) {
        forget_password_request_screen(return_page);
    }
}

static void forget_password_clear_runtime_state(void)
{
    if (forget_password_timeout_timer_handle != NULL) {
        xTimerDelete(forget_password_timeout_timer_handle, 0);
        forget_password_timeout_timer_handle = NULL;
    }
    if (forget_password_double_click_task_handle != NULL &&
        forget_password_double_click_task_handle != xTaskGetCurrentTaskHandle()) {
        vTaskDelete(forget_password_double_click_task_handle);
        forget_password_double_click_task_handle = NULL;
    }
    forget_password_state = WEBSERVER_FORGOT_PASSWORD_STATE_DONE;
    webserver_config_forget_password_state_notify();
    forget_password_socket_fd = 0;
    forget_password_restore_screen();
}

void check_and_clear_forget_password_socket(int fd)
{
    if (fd == forget_password_socket_fd) forget_password_clear_runtime_state();
}

static cJSON *forget_password_create_state_data(void)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        ESP_LOGE(TAG, "Failed to create data object");
        return NULL;
    }
    cJSON_AddStringToObject(data,
                           "state",
                           forget_password_state_to_string(webserver_config_forget_password_get_state()));
    return data;
}

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
void webserver_config_forget_password_cancel_timer(int set_password_fd)
{
    if (forget_password_state != WEBSERVER_FORGOT_PASSWORD_STATE_DONE) {
        forget_password_clear_runtime_state();
        webserver_close_current_client(set_password_fd);
    }
}

/**
 * @brief Notifies the waiting task that a double-click event has occurred.
 *
 * If the password reset waiting task is running, this function:
 * - Sets an RGB LED to the password reset indication.
 * - Sends a notification to the task indicating a double-click event.
 */
void webserver_config_forget_password_double_click(void)
{
    if (forget_password_double_click_task_handle != NULL) {
        // rgb_set_indicator_effect(RGB_RESET_PASSWORD);
        // ESP_LOGI(TAG, "RGB_RESET_PASSWORD");
        xTaskNotify(forget_password_double_click_task_handle, BUTTON_DOUBLE_CLICK, eSetValueWithOverwrite);
    }
}

/**
 * @brief Callback function for the password reset timeout timer.
 *
 * If the password reset process is still active when the timer expires,
 * this function:
 * - Sends a notification indicating the end of the reset window.
 * - Resets the start flag.
 * - Closes the current WebServer client connection if valid.
 *
 * @param[in] xTimer Handle to the expired timer.
 */
static void forget_password_timeout_detect_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    if (forget_password_state != WEBSERVER_FORGOT_PASSWORD_STATE_DONE) {
        forget_password_state = WEBSERVER_FORGOT_PASSWORD_STATE_DONE;
        xTimerDelete(xTimer, 0);
        forget_password_timeout_timer_handle = NULL;
        int socket_fd = forget_password_socket_fd;
        webserver_config_forget_password_state_notify();
        forget_password_socket_fd = 0;
        if (socket_fd > 0) {
            webserver_close_current_client(socket_fd);
        }
        forget_password_restore_screen();
    }
}

/**
 * @brief Task that waits for a double-click button event within a timeout.
 *
 * This function waits for up to 10 seconds for a `BUTTON_DOUBLE_CLICK` event.
 * - If the event is received, it notifies the WebServer that the password reset is ready,
 *   and starts a 5-minute timeout timer.
 * - If no event is received within the timeout, it notifies the WebServer that the reset process is done,
 *   resets internal flags, and closes the current client connection.
 *
 * This function is intended to run as a FreeRTOS task and deletes itself upon completion.
 *
 * @param[in] arg Unused task argument.
 */
static void forget_password_wait_double_click_task(void *arg)
{
    ESP_LOGI(TAG, "Wait for 10 seconds and double-click the button...");
    uint32_t event;
    bool ready = false;
    if (xTaskNotifyWait(0x00, ULONG_MAX, &event, pdMS_TO_TICKS(WAIT_TIMEOUT_MS)) == pdTRUE) {
        if (event == BUTTON_DOUBLE_CLICK) {
            ready = true;
            ESP_LOGI(TAG, "Detected double-click button, sent WebServer notification");
            forget_password_state = WEBSERVER_FORGOT_PASSWORD_STATE_READY;
            forget_password_request_screen(SCREEN_PAGE_RESET_PASSWORD);
            webserver_config_forget_password_state_notify();
            forget_password_timeout_timer_handle =
                xTimerCreate("forgot_pswd", pdMS_TO_TICKS(300000), pdFALSE, 0, forget_password_timeout_detect_callback);
            if (!forget_password_timeout_timer_handle ||
                xTimerStart(forget_password_timeout_timer_handle, 25) != pdPASS) {
                ready = false;
                if (forget_password_timeout_timer_handle) {
                    xTimerDelete(forget_password_timeout_timer_handle, 0);
                    forget_password_timeout_timer_handle = NULL;
                }
            }
        }
    }
    if (!ready) {
        forget_password_state = WEBSERVER_FORGOT_PASSWORD_STATE_DONE;
        int socket_fd = forget_password_socket_fd;
        webserver_config_forget_password_state_notify();
        forget_password_socket_fd = 0;
        if (socket_fd > 0) webserver_close_current_client(socket_fd);
        forget_password_restore_screen();
    }

    forget_password_double_click_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Starts the double-click waiting task if it's not already running.
 *
 * This function creates a new FreeRTOS task (`forget_password_wait_double_click_task`)
 * to monitor for a password reset trigger via button double-click. The task will only be started
 * if it is not already active.
 */
static bool forget_password_start_waiting_double_click_task(void)
{
    if (forget_password_double_click_task_handle == NULL) {
        return xTaskCreatePinnedToCore(forget_password_wait_double_click_task, "wait_double_click", 2560, NULL, 5,
                                       &forget_password_double_click_task_handle, 1) == pdPASS;
    }
    return false;
}

bool webserver_config_forget_password_is_authorized(int socket_fd)
{
    return forget_password_state == WEBSERVER_FORGOT_PASSWORD_STATE_READY &&
           socket_fd == forget_password_socket_fd;
}

webserver_forgot_password_state_t webserver_config_forget_password_get_state(void)
{
    return forget_password_state;
}

esp_err_t webserver_config_forget_password_state_notify(void)
{
    if (forget_password_socket_fd <= 0) {
        return ESP_OK;
    }
    return webserver_config_forget_password_state_notify_to_client(forget_password_socket_fd);
}

esp_err_t webserver_config_forget_password_state_notify_to_client(int socket_fd)
{
    if (socket_fd <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (socket_fd != forget_password_socket_fd) {
        return ESP_OK;
    }

    cJSON *data = forget_password_create_state_data();
    if (!data) {
        return ESP_ERR_NO_MEM;
    }
    return webserver_data_response_to_special_client(
        data, WEB_API_METHOD_SYS_FOTGET_PASS_NOTIFY, 0, socket_fd);
}

screen_page_t webserver_config_forget_password_get_screen_page(void)
{
    webserver_forgot_password_state_t state = webserver_config_forget_password_get_state();
    if (state == WEBSERVER_FORGOT_PASSWORD_STATE_DONE) {
        return SCREEN_PAGE_NONE;
    }
    return state == WEBSERVER_FORGOT_PASSWORD_STATE_READY
               ? SCREEN_PAGE_RESET_PASSWORD
               : SCREEN_PAGE_FORGOT_PASSWORD;
}

/**
 * @brief Parses JSON data for "forgot password" configuration.
 *
 * This function extracts the "state" and optionally the "duration" field from the input cJSON object.
 * If the state matches `SET_FORGOT_PASSWORD_WAITING`, the duration value is also parsed.
 *
 * @param[in]  root        Pointer to the root cJSON object containing the parsed JSON.
 * @param[out] state       Pointer to a buffer to receive the parsed state string.
 * @param[in]  state_size  Size of the state buffer.
 * @param[out] duration    Pointer to an integer to receive the parsed duration (if applicable).
 *
 * @retval ESP_OK          Successfully parsed the JSON data.
 * @retval ESP_FAIL        One or more parameters were invalid or parsing failed.
 *
 * @note The function ensures the `state` string is null-terminated.
 *       The caller must ensure the `state` buffer is valid and has sufficient size.
 */
static int webserver_config_forget_password_parse_cjson_data(cJSON *root, char *state, size_t state_size, int *duration)
{
    if (!root) {
        ESP_LOGE(TAG, "Parsing failed: parameter is empty");
        return ESP_FAIL;
    }

    char *string;
    if (cjson_parse_string_field(root, "state", &string)) {
        strlcpy(state, string, state_size);
    }
    if (strcmp(state, SET_FORGOT_PASSWORD_WAITING) == 0) {
        if (cjson_parse_int_field(root, "duration", duration)) {
            ESP_LOGI(TAG, "Analysis successful: state=%s, duration=%d", state, *duration);
        }
    }

    ESP_LOGI(TAG, "Analysis successful: state=%s", state);
    return ESP_OK;
}

/**
 * @brief Handle the "set forgot password" API request.
 *
 * Parses the forgot password request state and duration from the JSON data.
 * Depending on the state value, it either initiates or ends the forgot password process.
 *
 * States handled:
 *  - SET_FORGOT_PASSWORD_DONE: Marks the forgot password process as done and closes the current client connection.
 *  - SET_FORGOT_PASSWORD_WAITING: Starts a waiting timer for a double click event and marks the process as active.
 *  - Other states while process is active: Returns an error and closes the client connection.
 *
 * @param data           Pointer to the cJSON object containing the forgot password request data.
 * @param now_resp_arg   Pointer to the async response argument struct for client response.
 *
 * @return
 *      - ESP_OK if the request was handled successfully.
 *      - Appropriate error code on failure or invalid state.
 */
esp_err_t method_api_handle_set_forgot_password(cJSON *data, async_resp_arg *now_resp_arg)
{
    char state[8] = {0};
    int duration = 0;
    int error_code = WEB_API_ERROR_FAIL;

    if (webserver_config_forget_password_parse_cjson_data(data, state, sizeof(state), &duration) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse forgot password data");
        goto send_response;
    }
    if (forget_password_state != WEBSERVER_FORGOT_PASSWORD_STATE_DONE &&
        now_resp_arg->fd != forget_password_socket_fd) {
        ESP_LOGW(TAG, "setForgotPassword state invalid during active process");
        error_code = WEB_API_ERROR_SET_FORGET_PASSWORD;
    }
    else if (strcmp(state, SET_FORGOT_PASSWORD_DONE) == 0) {
        ESP_LOGW(TAG, "setForgotPassword state is done");
        forget_password_clear_runtime_state();
        error_code = WEB_API_ERROR_OK;
    }
    else if (strcmp(state, SET_FORGOT_PASSWORD_WAITING) == 0) {
        ESP_LOGW(TAG, "setForgotPassword state is waiting");
        if (forget_password_state != WEBSERVER_FORGOT_PASSWORD_STATE_DONE &&
            now_resp_arg->fd == forget_password_socket_fd) {
            error_code = WEB_API_ERROR_OK;
            goto send_response;
        }

        forget_password_capture_return_screen();
        forget_password_state = WEBSERVER_FORGOT_PASSWORD_STATE_WAITING;
        forget_password_socket_fd = now_resp_arg->fd;
        if (forget_password_start_waiting_double_click_task()) {
            error_code = WEB_API_ERROR_OK;
            forget_password_request_screen(SCREEN_PAGE_FORGOT_PASSWORD);
            webserver_config_forget_password_state_notify();
        }
        else {
            forget_password_clear_runtime_state();
        }
    }
send_response:
    webserver_code_response_to_current_client(error_code, WEB_API_METHOD_SYS_SET_FORGOT_PASSWORD_RSP,
                                              now_resp_arg->seq);
    return error_code;
}
