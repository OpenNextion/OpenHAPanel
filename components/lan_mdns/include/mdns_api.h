#ifdef __cplusplus
extern "C" {

#endif

#ifndef OPEN_NEXTION_COMPONENTS_LAN_MDNS_INCLUDE_MDNS_API_H
#define OPEN_NEXTION_COMPONENTS_LAN_MDNS_INCLUDE_MDNS_API_H

#include "mdns.h"
#include "webserver_method_api.h"
#include "wireless_controller.h"

#define HOSTNAME_MAX_LEN (64)
#define MDNS_ID_BUF_LEN (16)
#define MDNS_IP_LEN (16)

#define MDNS_OK (0)
#define MDNS_FAIL (-1)
#define MDNS_ERR_PARAM (-2)
#define MDNS_ERR_CONFLICT (-3)
#define MDNS_ERR_DOMAIN_CONFLICT (410)

#define MDNS_SCAN_TIME_MS 3000
#define MDNS_SCAN_INTERVAL_MS 5000

#define MDNS_CONFIG_FILE_NAME "mdns.conf"
#define DEFAULTS_HOST_NAME "nextion"
#define INSTANCE_NAME_PREFIX "Open Nextion"
#define INSTANCE_NAME_ROUTER_PREFIX "Open Nextion ZBRouter"
#define INSTANCE_NAME_THREAD_PREFIX "Open Nextion OpenThread"
#define INSTANCE_NAME_CUSTOM_PREFIX "Open Nextion"
#define MDNS_DST ".local"

typedef struct
{
    char hostname[MDNS_NAME_BUF_LEN];
    char ip_addr[MDNS_IP_LEN];
    char txt_username[MDNS_NAME_BUF_LEN];
    char txt_id[MDNS_ID_BUF_LEN];
} mdns_scan_result;

bool mdns_server_get_hostname(char *hostname, size_t len);

/**
 * @brief Refreshes the TXT records for the HTTP mDNS service.
 *
 * Updates the "_http._tcp" service's mDNS metadata with current product information.
 * Should be called whenever the product name or ID changes at runtime.
 */
void mdns_server_update_server_txt();

/**
 * @brief Updates service instance names and ZHA discovery for the active radio firmware.
 *
 * The _zigbee-coordinator._tcp service is advertised only in coordinator mode.
 */
esp_err_t mdns_server_sync_device_mode(wireless_controller_work_mode_t mode, uint32_t baud_rate);

/* Choose the configured name after obtaining an IP; use the MAC name on conflict. */
void mdns_select_hostname_for_network(void);
esp_err_t mdns_apply_fallback_hostname(void);
void mdns_verify_hostname(void);

/**
 * @brief Web API handler for setting the device's mDNS domain name.
 *
 * Parses and validates the "mdnsDomain" field from a JSON object, then
 * sets the hostname for mDNS if the domain format is correct and not in conflict.
 *
 * @param data           Pointer to cJSON object containing the input payload.
 * @param now_resp_arg   Pointer to structure for response sequencing.
 *
 * @return esp_err_t
 *         ESP_OK   - if hostname is successfully set
 *         ESP_FAIL - if input is invalid or setting hostname fails
 */
esp_err_t method_api_handle_set_mdns_domain(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Notifies the webserver with the current cached mDNS scan results.
 *
 * This function prepares a JSON payload containing the local device information
 * as well as previously scanned mDNS devices stored in `mdns_prev_scan_results`.
 * It sends the data to the web interface using `webserver_data_response_to_all_client()` with the
 * event identifier "Device.LanDevicesNotify".
 *
 */
void webserver_cur_mdns_scan_result_notify(void);

/**
 * @brief Load mDNS configuration from a JSON file.
 *
 * This function reads the mDNS configuration from a predefined file,
 * parses the JSON content, and extracts the hostname value.
 * If the file is missing, empty, or contains invalid JSON, it falls back
 * to updating the mDNS server with the default or current hostname.
 *
 * @return int8_t
 *         0  - Configuration loaded successfully or file is empty/not found (uses default hostname).
 *         -1 - Failed to parse JSON or invalid hostname in the config file.
 */
int8_t mdns_load_config_from_file(void);

#endif // OPEN_NEXTION_COMPONENTS_LAN_MDNS_INCLUDE_MDNS_API_H

#ifdef __cplusplus
}
#endif
