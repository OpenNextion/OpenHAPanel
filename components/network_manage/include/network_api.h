#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_API_H
#define OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_API_H

#include "esp_err.h"
#include "cJSON.h"
#include "webserver.h"
#include "network.h"
#include <stdbool.h>
#include <esp_wifi.h>

/**
 * @brief Set the WiFi Access Point password from encrypted input.
 *
 * @param data JSON containing encrypted password data.
 * @param now_resp_arg Async response argument with client info.
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t method_api_handle_net_set_ap_pass(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Switch WiFi connection using encrypted credentials.
 *
 * @param data JSON with encrypted WiFi parameters.
 * @param now_resp_arg Async response data.
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t method_api_handle_net_switch_wifi(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Trigger WiFi scan for a given duration.
 *
 * @param data JSON with scan parameters.
 * @param now_resp_arg Async response data.
 * @return ESP_OK if scan started, ESP_FAIL if scan busy.
 */
esp_err_t method_api_handle_net_scan(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Trigger an asynchronous Home Assistant instance scan over mDNS.
 *
 * Results are broadcast with Network.HassInstanceScanResultNotify.
 *
 * @param data Optional JSON object containing duration in seconds (integer, 1-30; defaults to 3).
 * @param now_resp_arg Async response data containing the request sequence.
 * @return ESP_OK if the scan task started, otherwise ESP_FAIL.
 */
esp_err_t method_api_handle_net_scan_hass_instance(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Process JSON API request to configure Ethernet IP settings.
 *
 * @param data JSON object with Ethernet config.
 * @param now_resp_arg Async response info.
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t method_api_handle_net_set_eth_config(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Process a JSON API request to configure WiFi station IP settings.
 *
 * @param data JSON object with WiFi station config.
 * @param now_resp_arg Async response info.
 * @return ESP_OK on success, ESP_FAIL on error.
 */
esp_err_t method_api_handle_net_set_wifi_config(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Process a JSON API request to configure Access Point settings.
 *
 * @param data JSON object with AP config.
 * @param now_resp_arg Async response info.
 * @return ESP_OK after processing.
 */
esp_err_t method_api_handle_net_set_ap_config(cJSON *data, async_resp_arg *now_resp_arg);

esp_err_t method_api_handle_net_set_ap_ssid(cJSON *data, async_resp_arg *now_resp_arg);

/**
 * @brief Retrieve the current network mode.
 *
 * @return Current network mode (network_mode_t).
 */
network_mode_t network_get_current_mode(void);

/**
 * @brief Broadcast comprehensive network status information to all clients.
 *
 * Includes network mode, internet connectivity, mDNS domain, and interface details.
 */
void webserver_network_info_notify(void);

/**
 * @brief Broadcast current network mode and related info to all clients.
 *
 * Sends a JSON notification including mode-specific network data and internet connectivity.
 *
 * @param current_mode The current network mode identifier.
 */
void webserver_network_mode_info_notify(int current_mode);

/**
 * @brief Broadcast WiFi connection result to all clients.
 *
 * @param error Error code representing the WiFi connection result.
 */
void webserver_network_wifi_connect_result_notify(int error);

/**
 * @brief Notify all connected clients with WiFi scan results.
 *
 * Converts the scanned WiFi AP list into JSON and broadcasts it to connected clients.
 *
 * @param ap_list Pointer to the scanned WiFi AP list.
 * @param apCount Number of AP entries in the list.
 */
void webserver_network_wifi_scan_info_notify(wifi_ap_record_t *ap_list, uint16_t apCount);

#endif // OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_API_H

#ifdef __cplusplus
}
#endif
