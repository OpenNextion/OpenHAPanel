#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Every full-screen page managed by the device screen state machine. */
typedef enum {
    /** No page has been accepted by screen_ui yet. */
    SCREEN_PAGE_NONE = -1,
    SCREEN_PAGE_AP_PROVISIONING = 0,
    SCREEN_PAGE_HA_PROVISIONING,
    /** Waiting for the user to confirm password recovery by double-clicking the device key. */
    SCREEN_PAGE_FORGOT_PASSWORD,
    /** Physical confirmation succeeded; waiting for a new password from the browser. */
    SCREEN_PAGE_RESET_PASSWORD,
    SCREEN_PAGE_SPLIT_VIEW,
    SCREEN_PAGE_RESTARTING,
    SCREEN_PAGE_COUNT,
} screen_page_t;

typedef enum {
    /** A module requested a page; screen_ui must accept or reject it. */
    SCREEN_STATE_EVENT_TRANSITION_REQUESTED = 0,
    /** screen_ui accepted a request and the current page changed. */
    SCREEN_STATE_EVENT_CHANGED,
} screen_state_event_id_t;

typedef struct {
    screen_page_t current;
    screen_page_t requested;
    uint32_t generation;
} screen_state_transition_request_t;

typedef struct {
    screen_page_t previous;
    screen_page_t current;
} screen_state_changed_event_t;

ESP_EVENT_DECLARE_BASE(SCREEN_STATE_EVENT);

/** Initialize the state machine. This function is idempotent. */
esp_err_t screen_state_init(screen_page_t initial_page);

/**
 * Request a transition. The current page is not changed until screen_ui calls
 * screen_state_commit_transition() after validating and accepting the page.
 */
esp_err_t screen_state_request_transition(screen_page_t page);

/** Request that screen_ui arbitrate and render the current page again. */
esp_err_t screen_state_request_refresh(void);

/** Accept the latest request; publish CHANGED only when the page enum changes. */
esp_err_t screen_state_commit_transition(const screen_state_transition_request_t *request);

/** Reject the latest matching request without changing the current page. */
void screen_state_reject_transition(const screen_state_transition_request_t *request);

/** Return the accepted page, or SCREEN_PAGE_NONE before the first page is accepted. */
screen_page_t screen_state_get_current(void);

bool screen_state_is_initialized(void);
const char *screen_state_to_string(screen_page_t page);

#ifdef __cplusplus
}
#endif
