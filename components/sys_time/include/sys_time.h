#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_H
#define OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_H

#include "sys_time_api.h"
#include "webserver_method_api.h"

#define TIME_CONFIG_FILE_NAME "time.conf"
#define NTP_DOMAIN_MAX_LEN 49

/**
 * @brief Get the main and spare NTP server names.
 *
 * Copies the currently configured NTP server hostnames into the provided buffers.
 *
 * @param[out] main_name  Buffer to receive the main NTP server name.
 * @param[out] spare_name Buffer to receive the spare NTP server name.
 *
 * @return Returns 0 on success.
 */
int sys_time_get_ntp_server_name(char *main_name, char *spare_name);

/**
 * @brief Handle API request to set the timezone.
 *
 * Parses timezone from JSON data, updates system timezone info, saves it,
 * and initiates DST synchronization task.
 *
 * @param data          Pointer to cJSON object containing request data.
 * @param now_resp_arg  Pointer to async response argument for client communication.
 *
 * @return
 *      - ESP_OK (0) on success,
 *      - Error code otherwise.
 */
esp_err_t method_api_handle_set_timezone(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Handle API request to get the timezone information.
 *
 * Parses the "timeZone" field from the JSON request data, then starts a task
 * to query the timezone information from the server asynchronously.
 *
 * @param data          Pointer to cJSON object containing request data.
 * @param now_resp_arg  Pointer to async response argument for sending client response.
 *
 * @return
 *      - ESP_OK on successfully starting the query task,
 *      - ESP_FAIL if parsing fails or task creation fails.
 */
esp_err_t method_api_handle_get_timezone(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Handle API request to set NTP server addresses.
 *
 * Parses the "server1" and "server2" fields from the JSON request data,
 * updates the NTP server names accordingly, saves the updated configuration
 * to file, and notifies the device info subsystem upon success.
 *
 * @param data          Pointer to cJSON object containing request data.
 * @param now_resp_arg  Pointer to async response argument for client response.
 *
 * @return
 *      - ESP_OK on successful update and save,
 *      - ESP_FAIL on failure.
 */
esp_err_t method_api_handle_set_ntp_server(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Reload time configuration from storage and start DST synchronization task.
 *
 * This function loads saved time info from persistent storage and triggers
 * a network query to update DST information.
 */
void sys_time_reload_time_info(void);

/**
 * @brief Initialize system time service.
 *
 * This function initializes the system time by loading saved configuration,
 * starting SNTP, and creating a periodic timer for DST synchronization.
 */
void sys_time_init(void);

/**
 * @brief Re-issue the SNTP request after an interface obtained an IP address.
 *
 * Call this from the IP_EVENT handlers. Without it the SNTP client keeps the
 * retry schedule started at boot (when no address existed yet), which delays
 * the first successful sync -- and therefore the status bar clock -- by up to
 * one full SNTP_RETRY_TIMEOUT. Safe to call repeatedly.
 */
void sys_time_on_network_ready(void);

#endif // OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_H

#ifdef __cplusplus
}
#endif
