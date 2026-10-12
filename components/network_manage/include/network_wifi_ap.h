#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_WIFI_AP_H
#define OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_WIFI_AP_H

#include "esp_err.h"
#include <stdbool.h>
#include "network.h"

#define AP_MAX_CONNECT_NUM 8

/**
 * @brief Enable or disable internet connection sharing on the Access Point (AP).
 *
 * Updates the AP internet connection state, deauthenticates all connected stations
 * if needed, and notifies the webserver about the network mode change.
 *
 * @param enable Boolean flag to enable (true) or disable (false) internet connection.
 *
 * @return 0 on success, -1 on failure.
 */
int network_enable_ap_internet_connection(bool enable);

/**
 * @brief Start or stop the WiFi Access Point with specified connection parameters.
 *
 * Sets the AP active status, updates AP parameters such as SSID and password,
 * applies configuration including internet sharing state, saves settings,
 * and notifies the webserver of the network mode.
 *
 * @param password Parameters for the AP connection including password
 */
int network_wifi_ap_set_password(const uint8_t *password);

int network_wifi_ap_set_ssid(const char *ssid);
/**
 * @brief Activate or deactivate the WiFi Access Point.
 *
 * Compares current AP status with the requested state and performs enable/disable
 * actions if different. Manages internet sharing state and updates configuration.
 *
 * @param enable Boolean flag to activate (true) or deactivate (false) the AP.
 */
void network_wifi_ap_set_active(bool enable);

/**
 * @brief Enable or disable internet connection sharing on the Wi-Fi AP.
 *
 * @param[in] flag 1 to enable, 0 to disable internet connection sharing.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on failure to find SoftAP netif or set IP info.
 */
esp_err_t network_wifi_ap_internet_connection_enable(int flag);

/**
 * @brief Refresh AP uplink DNS/NAPT state without restarting SoftAP.
 *
 * This is used when the upstream interface changes or obtains a new IP.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t network_wifi_ap_refresh_uplink_state(void);

/**
 * @brief Get whether the Wi-Fi AP is currently enabled.
 *
 * @return true if AP is enabled, false otherwise.
 */
bool network_wifi_get_ap_enabled(void);

/**
 * @brief Disable the Wi-Fi Access Point to prevent client connections.
 *
 * @return ESP_OK on success.
 */
esp_err_t network_wifi_ap_disable_connection(void);

/**
 * @brief Enable the Wi-Fi Access Point to accept client connections.
 *
 * @return ESP_OK on success.
 */
esp_err_t network_wifi_ap_enable_connection(void);

esp_err_t network_wifi_ap_set_gateway_netmask(uint32_t gw_addr, uint32_t netmask_addr);

/**
 * @brief Get the Wi-Fi Access Point Name
 *
 * @param[out] out ap name
 *
 * @param[in] len length of ap name
 *
 */
void network_wifi_ap_get_name(char *out, size_t len);

/**
 * @brief Configure and enable/disable the Wi-Fi AP mode with specified SSID and password.
 *
 * If password is invalid (NULL or less than MIN_PASSWORD_LENGTH), open network is created.
 * Also handles timer control based on open/secured mode.
 *
 * @param[in] enable        If false, enables connection; if true, disables connection.
 * @param[in] ssid          Pointer to SSID string (can be NULL to use default).
 * @param[in] password      Pointer to password string (NULL or < MIN_PASSWORD_LENGTH means open AP).
 * @param[in] internet_connection  Not used in this function but may control external logic.
 * @param ssid_hidden
 * @param ap_channel
 *
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t network_wifi_ap_handler_set_mode(bool enable, const uint8_t *ssid, const uint8_t *password,
                                           bool internet_connection, bool ssid_hidden, uint32_t ap_channel);

esp_err_t network_wifi_ap_set_bandwidth(uint8_t band_width);

/**
 * @brief Initialize the Wi-Fi AP network interface and configure shutdown timer.
 *
 * This function sets up the AP interface and registers a timer that automatically
 * disables the AP if no device connects within 10 minutes.
 *
 * @return
 *      - ESP_OK on success
 *      - Error code on failure
 */
esp_err_t network_wifi_ap_init(void);

#endif // OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_WIFI_AP_H

#ifdef __cplusplus
}
#endif
