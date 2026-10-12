#pragma once

#include <stdbool.h>
#include <time.h>

#include "cJSON.h"
#include "esp_err.h"
#include "ha_entity_store_type.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_init(void);

/**
 * Lock or unlock Web UI operations for a screen-initiated firmware update.
 * The lock remains active across download, verification, activation and reboot.
 */
void app_set_manual_ota_web_lock(bool locked);

/** Borrow the application-owned Entity Store. May return NULL when HA is not configured. */
ha_entity_store_t *app_get_ha_entity_store(void);

/** Call a Home Assistant service for one entity through the app-owned client. */
esp_err_t app_ha_call_entity_service(const char *domain,
                                     const char *service,
                                     const char *entity_id);
/** Call a service with optional service_data. The payload is copied. */
esp_err_t app_ha_call_entity_service_data(const char *domain,
                                          const char *service,
                                          const char *entity_id,
                                          const cJSON *service_data);

/** Call a Home Assistant service with optional service_data for one entity. */
esp_err_t app_ha_call_entity_service_data(const char *domain,
                                          const char *service,
                                          const char *entity_id,
                                          const cJSON *service_data);

typedef void (*app_ha_history_result_cb_t)(int request_id,
                                           esp_err_t err,
                                           const cJSON *result,
                                           const cJSON *error,
                                           void *user_ctx);

/**
 * Fetch recorder history for one entity. The callback runs outside the HA
 * WebSocket lock and may safely acquire the LVGL lock.
 */
esp_err_t app_ha_get_entity_history(const char *entity_id,
                                    time_t start_time,
                                    time_t end_time,
                                    app_ha_history_result_cb_t callback,
                                    void *user_ctx,
                                    int *request_id_out);

#ifdef __cplusplus
}
#endif
