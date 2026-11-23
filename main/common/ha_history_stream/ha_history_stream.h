#pragma once

#include <stdbool.h>

#include "cJSON.h"
#include "esp_err.h"
#include "ha_ws_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ha_history_stream ha_history_stream_t;

/* Called on the LVGL timer thread. states contains one history/stream chunk. */
typedef void (*ha_history_stream_cb_t)(int subscription_id, esp_err_t err,
                                       const cJSON *states, bool reset,
                                       void *user_ctx);

esp_err_t ha_history_stream_open(ha_ws_client_t *client,
                                 const char *entity_id,
                                 unsigned hours_to_show,
                                 ha_history_stream_cb_t callback,
                                 void *user_ctx,
                                 ha_history_stream_t **stream_out,
                                 int *subscription_id_out);

/* Call from the LVGL thread before destroying the owning More Info screen. */
void ha_history_stream_close(ha_history_stream_t *stream);

/* Detach subscriptions before the application destroys its WebSocket client. */
void ha_history_stream_detach_client(ha_ws_client_t *client);
bool ha_history_stream_is_attached(const ha_history_stream_t *stream);

#ifdef __cplusplus
}
#endif
