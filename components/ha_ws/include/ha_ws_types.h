#pragma once

#include <stdbool.h>

#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ha_ws_client ha_ws_client_t;

typedef enum {
    HA_WS_EVENT_CONNECTED = 0,
    HA_WS_EVENT_AUTH_REQUIRED,
    HA_WS_EVENT_AUTHENTICATED,
    HA_WS_EVENT_AUTH_INVALID,
    HA_WS_EVENT_DISCONNECTED,
    HA_WS_EVENT_ERROR,
} ha_ws_event_t;

typedef void (*ha_ws_connection_cb_t)(ha_ws_client_t *client,
                                      ha_ws_event_t event,
                                      void *user_ctx);

typedef void (*ha_ws_result_cb_t)(ha_ws_client_t *client,
                                  int id,
                                  esp_err_t err,
                                  const cJSON *result,
                                  const cJSON *error,
                                  void *user_ctx);

typedef void (*ha_ws_event_cb_t)(ha_ws_client_t *client,
                                 int subscription_id,
                                 const cJSON *event,
                                 void *user_ctx);

typedef struct {
    const char *hass_url;
    const char *access_token;
    bool auto_reconnect;
    bool coalesce_messages;
    ha_ws_connection_cb_t connection_cb;
    void *user_ctx;
} ha_ws_config_t;

#ifdef __cplusplus
}
#endif
