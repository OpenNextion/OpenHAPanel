#include "screen_state.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

ESP_EVENT_DEFINE_BASE(SCREEN_STATE_EVENT);

static const char *TAG = "screen_state";
static portMUX_TYPE s_state_guard = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized;
static screen_page_t s_current_page = SCREEN_PAGE_NONE;
static screen_page_t s_pending_page = SCREEN_PAGE_AP_PROVISIONING;
static uint32_t s_request_generation;
static bool s_request_pending;

static bool screen_state_page_is_valid(screen_page_t page)
{
    return page >= SCREEN_PAGE_AP_PROVISIONING && page < SCREEN_PAGE_COUNT;
}

static esp_err_t screen_state_request_transition_internal(screen_page_t page, bool refresh);

esp_err_t screen_state_init(screen_page_t initial_page)
{
    if (!screen_state_page_is_valid(initial_page)) {
        return ESP_ERR_INVALID_ARG;
    }

    taskENTER_CRITICAL(&s_state_guard);
    if (s_initialized) {
        taskEXIT_CRITICAL(&s_state_guard);
        return ESP_OK;
    }
    s_current_page = SCREEN_PAGE_NONE;
    s_request_pending = false;
    s_initialized = true;
    taskEXIT_CRITICAL(&s_state_guard);

    return screen_state_request_transition(initial_page);
}

esp_err_t screen_state_request_transition(screen_page_t page)
{
    return screen_state_request_transition_internal(page, false);
}

esp_err_t screen_state_request_refresh(void)
{
    taskENTER_CRITICAL(&s_state_guard);
    screen_page_t page = s_current_page;
    bool can_refresh = s_initialized && screen_state_page_is_valid(page);
    bool request_pending = s_request_pending;
    taskEXIT_CRITICAL(&s_state_guard);

    if (request_pending) {
        return ESP_OK;
    }
    return can_refresh
               ? screen_state_request_transition_internal(page, true)
               : ESP_ERR_INVALID_STATE;
}

static esp_err_t screen_state_request_transition_internal(screen_page_t page, bool refresh)
{
    if (!screen_state_page_is_valid(page)) {
        return ESP_ERR_INVALID_ARG;
    }

    screen_state_transition_request_t request = {0};

    taskENTER_CRITICAL(&s_state_guard);
    if (!s_initialized) {
        taskEXIT_CRITICAL(&s_state_guard);
        return ESP_ERR_INVALID_STATE;
    }
    if (s_current_page == SCREEN_PAGE_RESTARTING) {
        taskEXIT_CRITICAL(&s_state_guard);
        return page == SCREEN_PAGE_RESTARTING ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    if (s_request_pending && s_pending_page == SCREEN_PAGE_RESTARTING) {
        taskEXIT_CRITICAL(&s_state_guard);
        return page == SCREEN_PAGE_RESTARTING ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    if (s_request_pending && s_pending_page == page) {
        taskEXIT_CRITICAL(&s_state_guard);
        return ESP_OK;
    }
    if (s_current_page == page && !refresh) {
        s_request_pending = false;
        taskEXIT_CRITICAL(&s_state_guard);
        return ESP_OK;
    }
    s_request_generation++;
    s_pending_page = page;
    s_request_pending = true;
    request.current = s_current_page;
    request.requested = page;
    request.generation = s_request_generation;
    taskEXIT_CRITICAL(&s_state_guard);

    esp_err_t err = esp_event_post(SCREEN_STATE_EVENT,
                                   SCREEN_STATE_EVENT_TRANSITION_REQUESTED,
                                   &request,
                                   sizeof(request),
                                   pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        screen_state_reject_transition(&request);
    }
    return err;
}

esp_err_t screen_state_commit_transition(const screen_state_transition_request_t *request)
{
    if (!request || !screen_state_page_is_valid(request->requested)) {
        return ESP_ERR_INVALID_ARG;
    }

    screen_state_changed_event_t event = {0};
    taskENTER_CRITICAL(&s_state_guard);
    if (!s_initialized || !s_request_pending ||
        request->generation != s_request_generation ||
        request->current != s_current_page ||
        request->requested != s_pending_page) {
        taskEXIT_CRITICAL(&s_state_guard);
        return ESP_ERR_INVALID_STATE;
    }
    event.previous = s_current_page;
    event.current = request->requested;
    s_current_page = request->requested;
    s_request_pending = false;
    taskEXIT_CRITICAL(&s_state_guard);

    if (event.previous == event.current) {
        ESP_LOGI(TAG, "Screen page refresh accepted: %s", screen_state_to_string(event.current));
        return ESP_OK;
    }

    ESP_LOGI(TAG,
             "Screen page changed: %s -> %s",
             screen_state_to_string(event.previous),
             screen_state_to_string(event.current));
    esp_err_t notify_err = esp_event_post(SCREEN_STATE_EVENT,
                                          SCREEN_STATE_EVENT_CHANGED,
                                          &event,
                                          sizeof(event),
                                          pdMS_TO_TICKS(100));
    if (notify_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish changed event: %s", esp_err_to_name(notify_err));
    }
    return ESP_OK;
}

void screen_state_reject_transition(const screen_state_transition_request_t *request)
{
    if (!request) {
        return;
    }

    taskENTER_CRITICAL(&s_state_guard);
    if (s_request_pending &&
        request->generation == s_request_generation &&
        request->requested == s_pending_page) {
        s_request_pending = false;
    }
    taskEXIT_CRITICAL(&s_state_guard);
}

screen_page_t screen_state_get_current(void)
{
    taskENTER_CRITICAL(&s_state_guard);
    screen_page_t page = s_current_page;
    taskEXIT_CRITICAL(&s_state_guard);
    return page;
}

bool screen_state_is_initialized(void)
{
    taskENTER_CRITICAL(&s_state_guard);
    bool initialized = s_initialized;
    taskEXIT_CRITICAL(&s_state_guard);
    return initialized;
}

const char *screen_state_to_string(screen_page_t page)
{
    switch (page) {
    case SCREEN_PAGE_NONE:
        return "none";
    case SCREEN_PAGE_AP_PROVISIONING:
        return "ap_provisioning";
    case SCREEN_PAGE_HA_PROVISIONING:
        return "ha_provisioning";
    case SCREEN_PAGE_FORGOT_PASSWORD:
        return "forgot_password";
    case SCREEN_PAGE_RESET_PASSWORD:
        return "reset_password";
    case SCREEN_PAGE_SPLIT_VIEW:
        return "split_view";
    case SCREEN_PAGE_RESTARTING:
        return "restarting";
    default:
        return "unknown";
    }
}
