#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "esp_err.h"
#include "ha_ws_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HA_WS_ERR_BASE 0x7000
#define HA_WS_ERR_NOT_AUTHENTICATED (HA_WS_ERR_BASE + 1)
#define HA_WS_ERR_NO_FREE_SLOT (HA_WS_ERR_BASE + 2)
#define HA_WS_ERR_INVALID_RESPONSE (HA_WS_ERR_BASE + 3)
#define HA_WS_ERR_AUTH_INVALID (HA_WS_ERR_BASE + 4)

esp_err_t ha_ws_client_init(const ha_ws_config_t *config, ha_ws_client_t **out_client);
/**
 * Start this client after obtaining the process-wide HA connection slot.
 * The call waits while another ha_ws client is started; ha_ws_close() or
 * ha_ws_destroy() releases the slot.
 */
esp_err_t ha_ws_connect(ha_ws_client_t *client);
/** Connect after waiting at most timeout_ms for the shared HA connection slot. */
esp_err_t ha_ws_connect_timeout(ha_ws_client_t *client, uint32_t timeout_ms);
esp_err_t ha_ws_close(ha_ws_client_t *client);
esp_err_t ha_ws_destroy(ha_ws_client_t *client);

bool ha_ws_is_authenticated(ha_ws_client_t *client);
const char *ha_ws_get_ha_version(ha_ws_client_t *client);

esp_err_t ha_ws_send_command(ha_ws_client_t *client,
                             cJSON *message,
                             ha_ws_result_cb_t cb,
                             void *user_ctx,
                             int *id_out);

esp_err_t ha_ws_get_states(ha_ws_client_t *client,
                           ha_ws_result_cb_t cb,
                           void *user_ctx,
                           int *id_out);

esp_err_t ha_ws_get_config(ha_ws_client_t *client,
                           ha_ws_result_cb_t cb,
                           void *user_ctx,
                           int *id_out);

esp_err_t ha_ws_get_services(ha_ws_client_t *client,
                             ha_ws_result_cb_t cb,
                             void *user_ctx,
                             int *id_out);

esp_err_t ha_ws_get_user(ha_ws_client_t *client,
                         ha_ws_result_cb_t cb,
                         void *user_ctx,
                         int *id_out);

esp_err_t ha_ws_ping(ha_ws_client_t *client,
                     ha_ws_result_cb_t cb,
                     void *user_ctx,
                     int *id_out);

esp_err_t ha_ws_call_service(ha_ws_client_t *client,
                             const char *domain,
                             const char *service,
                             const cJSON *service_data,
                             const cJSON *target,
                             bool return_response,
                             ha_ws_result_cb_t cb,
                             void *user_ctx,
                             int *id_out);

esp_err_t ha_ws_subscribe_events(ha_ws_client_t *client,
                                 const char *event_type,
                                 ha_ws_event_cb_t event_cb,
                                 ha_ws_result_cb_t result_cb,
                                 void *user_ctx,
                                 int *subscription_id_out);

/**
 * Subscribe to the current state and subsequent compact updates for only the
 * supplied Home Assistant entities.
 */
esp_err_t ha_ws_subscribe_entities(ha_ws_client_t *client,
                                   const char *const *entity_ids,
                                   size_t entity_id_count,
                                   ha_ws_event_cb_t event_cb,
                                   ha_ws_result_cb_t result_cb,
                                   void *user_ctx,
                                   int *subscription_id_out);

esp_err_t ha_ws_unsubscribe_events(ha_ws_client_t *client,
                                   int subscription_id,
                                   ha_ws_result_cb_t cb,
                                   void *user_ctx,
                                   int *id_out);

/**
 * Remove all event subscriptions matching event_cb and event_user_ctx.
 *
 * Unlike ha_ws_unsubscribe_events(), this remains safe after an automatic
 * reconnect, where the Home Assistant subscription command id may have
 * changed internally. The local callback is detached before this function
 * returns. If the client is authenticated, an unsubscribe command is also
 * sent to Home Assistant.
 */
esp_err_t ha_ws_unsubscribe_events_by_callback(ha_ws_client_t *client,
                                               ha_ws_event_cb_t event_cb,
                                               void *event_user_ctx);

/** Detach callbacks for in-flight commands that use the supplied context. */
esp_err_t ha_ws_cancel_pending_by_context(ha_ws_client_t *client, void *user_ctx);

#ifdef __cplusplus
}
#endif
