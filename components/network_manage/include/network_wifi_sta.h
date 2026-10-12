#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_WIFI_STA_H
#define OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_WIFI_STA_H

#include "network.h"
#include <stdbool.h>
#include <esp_netif_types.h>
#include <esp_event.h>

/**
 * @brief Get the current Wi-Fi scan status.
 *
 * Returns whether a Wi-Fi scan is currently in progress.
 *
 * @return int32_t 1 if a scan is ongoing, 0 otherwise.
 */
int32_t network_get_if_wifi_scan(void);

/**
 * @brief Start a Wi-Fi scan with the specified duration.
 *
 * Pauses the STA retry loop then initiates the scan. If the STA was actively
 * connecting the scan is deferred until WIFI_EVENT_STA_DISCONNECTED confirms
 * the radio is idle.
 *
 * @param[in] duration Scan duration in seconds. Internally scaled for timing.
 */
void network_wifi_scan(int duration);

/**
 * @brief Start a Wi-Fi scan immediately (no retry-pause).
 *
 * Called either directly from network_wifi_scan() when the STA is already
 * idle/connected, or from the disconnect handler after a deferred scan was
 * queued by network_sta_retry_pause_for_scan().
 *
 * @param[in] duration Scan duration in seconds.
 */
void network_wifi_scan_start(int duration);

/**
 * @brief Wi-Fi STA scan completion event handler.
 *
 * This function is triggered when a Wi-Fi scan completes. It retrieves the list
 * of detected access points (APs) and sends the scan results to the web server.
 *
 * @param[in] arg User-provided argument (unused).
 * @param[in] event_base Event base (should be WIFI_EVENT).
 * @param[in] event_id Event ID (should be WIFI_EVENT_SCAN_DONE).
 * @param[in] event_data Event data (unused).
 */
void network_sta_scan_done_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**
 * @brief Set a static IP configuration for the Wi-Fi STA interface.
 *
 * Stops the DHCP client and applies a manually defined static IP, netmask,
 * gateway, and DNS server configuration to the station interface.
 *
 * @param[in] ip The static IP configuration structure to apply.
 */
void network_wifi_sta_set_static_ip(network_ip_config_t ip);

/**
 * @brief Restart the DHCP client on the Wi-Fi station interface.
 *
 * Stops and then immediately starts the DHCP client for the STA interface.
 */
void network_wifi_sta_restart_dhcp(void);

/**
 * @brief Stop the DHCP client on the Wi-Fi STA interface.
 *
 * This function stops the DHCP client on the Wi-Fi station
 */
void network_wifi_sta_stop_dhcp(void);

/**
 * @brief Get the primary (DNS1) IPv4 DNS server address assigned to the STA interface.
 *
 * @return uint32_t The IPv4 address of the primary DNS server in network byte order.
 */
uint32_t network_wifi_sta_get_dns1(void);

/**
 * @brief Get the secondary (DNS2) IPv4 DNS server address assigned to the STA interface.
 *
 * @return uint32_t The IPv4 address of the secondary DNS server in network byte order.
 */
uint32_t network_wifi_sta_get_dns2(void);

/**
 * @brief Configure and initiate a connection to a Wi-Fi station (STA).
 *
 * This function sets the SSID, password, and BSSID (optional) for the
 * Wi-Fi STA interface and initiates the connection process.
 *
 * @param[in] params Pointer to a structure containing Wi-Fi connection parameters.
 * @return ESP_OK if the connection attempt was started, otherwise an ESP-IDF error.
 */
esp_err_t network_wifi_sta_connect(network_sta_connect_params_t *params);

/** Consume the disconnect event intentionally generated while switching STA config. */
bool network_wifi_sta_take_intentional_disconnect(void);

/**
 * @brief Disconnect to a specified Wi-Fi station (STA).
 *
 * This function performs the following actions:
 * - Applies the configuration using ESP-IDF Wi-Fi APIs.
 * - Disconnects any existing connection.
 *
 * @param[in] params void
 */
void network_wifi_sta_disconnect();

/**
 * @brief Start the process of connecting to a Wi-Fi station (STA) network.
 *
 * This function initializes the Wi-Fi STA connection using the provided
 * parameters, manages DHCP behavior based on Ethernet state, and starts
 * the connection process.
 *
 * @param[in] params Wi-Fi connection parameters such as SSID and password.
 * @return ESP_OK if the connection attempt was started, otherwise an ESP-IDF error.
 */
esp_err_t network_wifi_sta_start_connect(network_sta_connect_params_t params);

/**
 * @brief Apply and persist the selected Wi-Fi station IP mode.
 *
 * Restarts DHCP when DHCP mode is selected. In static mode, stores the
 * validated address and applies it immediately when the station is connected.
 * The caller is responsible for validating the address and checking conflicts.
 *
 * @param[in] ip The IP configuration structure containing static IP info.
 */
void network_wifi_sta_apply_ip_mode(network_ip_config_t ip);

esp_netif_t *network_sta_get_netif(void);

void network_wifi_sta_update_hostname();

/**
 * @brief Initialize the Wi-Fi station interface and set a unique hostname.
 *
 * This function sets up the default Wi-Fi station interface using ESP-IDF
 * APIs and assigns a hostname in the format "nextion-xxxx", where "xxxx"
 * represents the lowercase last two bytes of the device's MAC address.
 */
void network_wifi_sta_init(void);

#endif // OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_WIFI_STA_H

#ifdef __cplusplus
}
#endif