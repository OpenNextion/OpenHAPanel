#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"
#include "ha_ws_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HA_AUTH_URL_MAX_LEN 256
#define HA_AUTH_TOKEN_MAX_LEN 768
#define HA_AUTH_AUTHORIZE_URL_MAX_LEN 1024

/**
 * Home Assistant authorization events. Event payloads are intentionally empty;
 * handlers should call ha_auth_get_connection_info() for a fresh snapshot.
 */
ESP_EVENT_DECLARE_BASE(HA_AUTH_EVENT);

typedef enum {
    /** Authorization URL, token, configured or enabled state changed. */
    HA_AUTH_EVENT_CONNECTION_INFO_CHANGED = 0,
    /** Runtime connection status changed. */
    HA_AUTH_EVENT_STATUS_CHANGED,
    /** The current HA WebSocket must be released for an exclusive auth operation. */
    HA_AUTH_EVENT_CONNECTION_SUSPEND_REQUESTED,
    /** A newly created long-lived token was verified and persisted. */
    HA_AUTH_EVENT_LONG_LIVED_TOKEN_CREATED,
} ha_auth_event_id_t;

typedef enum {
    HA_AUTH_STATUS_NOT_CONFIGURED = 0,
    HA_AUTH_STATUS_AUTHORIZING,
    HA_AUTH_STATUS_EXCHANGING_CODE,
    HA_AUTH_STATUS_CREATING_LONG_LIVED_TOKEN,
    HA_AUTH_STATUS_DISABLED,
    HA_AUTH_STATUS_CONNECTING,
    HA_AUTH_STATUS_CONNECTED,
    HA_AUTH_STATUS_DISCONNECTED,
    HA_AUTH_STATUS_ERROR,
} ha_auth_status_t;

typedef enum {
    HA_AUTH_REASON_NONE = 0,
    HA_AUTH_REASON_INVALID_PARAM,
    HA_AUTH_REASON_INVALID_URL,
    HA_AUTH_REASON_OAUTH_BUSY,
    HA_AUTH_REASON_STATE_MISMATCH,
    HA_AUTH_REASON_STATE_EXPIRED,
    HA_AUTH_REASON_CODE_EXCHANGE_FAILED,
    HA_AUTH_REASON_HA_WS_CONNECT_FAILED,
    HA_AUTH_REASON_LONG_LIVED_TOKEN_FAILED,
    HA_AUTH_REASON_STORAGE_FAILED,
    HA_AUTH_REASON_NOT_CONFIGURED,
    /** The saved HA access token was rejected by Home Assistant. */
    HA_AUTH_REASON_AUTH_INVALID,
    /** The HA WebSocket reported a non-authentication connection error. */
    HA_AUTH_REASON_CONNECTION_ERROR,
} ha_auth_reason_t;

typedef struct {
    bool configured;
    bool enabled;
    bool device_connected;
    char hass_url[HA_AUTH_URL_MAX_LEN];
    char long_lived_token[HA_AUTH_TOKEN_MAX_LEN];
    uint32_t token_generated_at;
    ha_auth_status_t status;
    ha_auth_reason_t reason;
} ha_auth_connection_info_t;

typedef void (*ha_auth_authorization_cb_t)(esp_err_t err,
                                           ha_auth_reason_t reason,
                                           const ha_auth_connection_info_t *info,
                                           void *user_ctx);

typedef void (*ha_auth_status_cb_t)(const ha_auth_connection_info_t *info, void *user_ctx);

typedef void (*ha_auth_update_url_cb_t)(esp_err_t err,
                                        ha_auth_reason_t reason,
                                        bool changed,
                                        const ha_auth_connection_info_t *info,
                                        void *user_ctx);

esp_err_t ha_auth_init(void);
void ha_auth_set_status_callback(ha_auth_status_cb_t callback, void *user_ctx);

esp_err_t ha_auth_start_oauth(const char *hass_url,
                              const char *redirect_uri,
                              char *authorize_url,
                              size_t authorize_url_size,
                              uint32_t *expires_in,
                              ha_auth_reason_t *reason);

esp_err_t ha_auth_complete_oauth(const char *code,
                                 const char *state,
                                 ha_auth_authorization_cb_t callback,
                                 void *user_ctx,
                                 ha_auth_reason_t *reason);

esp_err_t ha_auth_get_connection_info(ha_auth_connection_info_t *info);

/** Update authorization status from the application-owned HA WebSocket. */
void ha_auth_handle_ws_event(ha_ws_event_t event);
esp_err_t ha_auth_update_url(const char *hass_url,
                             ha_auth_update_url_cb_t callback,
                             void *user_ctx,
                             ha_auth_reason_t *reason);
esp_err_t ha_auth_activate(ha_auth_reason_t *reason);
esp_err_t ha_auth_disable(void);
esp_err_t ha_auth_remove_authorization(void);
esp_err_t ha_auth_prepare_factory_reset(void);

const char *ha_auth_status_to_string(ha_auth_status_t status);
const char *ha_auth_reason_to_string(ha_auth_reason_t reason);

#ifdef __cplusplus
}
#endif
