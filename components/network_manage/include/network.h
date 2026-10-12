#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_H
#define OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_H

#include <stdbool.h>
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/dhcp.h"

/** Default IPv4 address and gateway used by the provisioning SoftAP. */
#define NETWORK_DEFAULT_AP_IP "192.168.4.1"

/** Default SSID prefix used by the provisioning SoftAP. */
#define NETWORK_DEFAULT_AP_SSID_PREFIX "Nextion"

/**
 * @enum network_mode_t
 * @brief Network mode enumeration.
 */
typedef enum
{
    NET_MODE_NULL,          /**< No network mode */
    NET_MODE_AP_ONLY,       /**< Access Point (AP) only mode */
    NET_MODE_ETHERNET_ONLY, /**< Ethernet only mode */
    NET_MODE_WLAN_ONLY,     /**< WLAN only mode */
    NET_MODE_ETHERNET_AP,   /**< Ethernet + AP mode */
    NET_MODE_WLAN_AP        /**< WLAN + AP mode */
} network_mode_t;

/**
 * @enum network_ip_config_mode_t
 * @brief IP configuration mode enumeration.
 */
typedef enum
{
    IP_MODE_DHCP_AUTO = 0, /**< DHCP automatic configuration */
    IP_MODE_STATIC = 1,    /**< Manual/static IP configuration */
} network_ip_config_mode_t;

/**
 * @struct network_ip_config_t
 * @brief IPv4 configuration used by DHCP status and static addressing.
 */
typedef struct
{
    bool is_valid;    /**< Validity flag */
    uint32_t ip;      /**< IP address */
    uint32_t netmask; /**< Subnet mask */
    uint32_t gateway; /**< Gateway address */
    uint32_t dns1;    /**< Primary DNS server */
    uint32_t dns2;    /**< Secondary DNS server */
} network_ip_config_t;

/**
 * @struct network_sta_connect_params_t
 * @brief WLAN station connection parameters.
 */
typedef struct
{
    uint8_t ssid[33];     /**< SSID of the Wi-Fi network */
    uint8_t password[65]; /**< Password of the Wi-Fi network */
    uint8_t bssid[6];     /**< BSSID (MAC address) of the AP */
} network_sta_connect_params_t;

/**
 * @struct network_ap_config_t
 * @brief AP parameters.
 */
typedef struct
{
    bool active;           /**< Current AP (Access Point) runtime state */
    bool default_active;   /**< User-configured persistent AP default state */
    bool internet_connect; /**< AP internet connectivity enabled flag */
    bool hide_ssid;        /**< AP hide SSID flag */
    uint32_t channel;      /**< AP channel */
    uint32_t band_width;   /**< AP bandwidth */
} network_ap_config_t;

/**
 * @struct NetworkState
 * @brief Global network state structure.
 */
typedef struct
{
    network_mode_t current_mode;   /**< Current network mode */
    network_ap_config_t ap_config; /**< AP configuration */
    bool eth_start;                /**< Ethernet interface started flag */
    bool eth_connected;            /**< Ethernet connection status flag */
    bool sta_connect_start_flag;   /**< Wi-Fi STA connection start flag */
    bool sta_connected_flag;       /**< Wi-Fi STA connected flag */
    bool internet_connected;       /**< External internet connectivity status */

    network_sta_connect_params_t wifi_params;      /**< Wi-Fi connection credentials */
    network_sta_connect_params_t temp_wifi_params; /**< Temporary Wi-Fi credentials to replace main on success */
    network_sta_connect_params_t ap_params;        /**< AP credentials */

    network_ip_config_mode_t eth_ip_mode; /**< Ethernet IP configuration mode: DHCP or static */
    network_ip_config_mode_t sta_ip_mode; /**< STA IP configuration mode: DHCP or static */

    network_ip_config_t sta_static_ip; /**< Static IP configuration for STA */
    network_ip_config_t eth_static_ip; /**< Static IP configuration for Ethernet */
    network_ip_config_t current_ip;    /**< Current uplink IP configuration */
    network_ip_config_t current_ap_ip; /**< Current AP IP configuration */
} NetworkState;

extern NetworkState net_state;

/**
 * @brief Get pointer to the global network state structure.
 *
 * @return Pointer to NetworkState instance.
 */
NetworkState *network_get_state(void);

/**
 * @brief Set the STA connection start flag in the global network state.
 *
 * @param flag Boolean value to set the STA connect start flag.
 */
void network_set_sta_connect_start_flag(bool flag);

/** Clear retry/backoff state left by the previous STA connection attempt. */
void network_reset_sta_retry_state(void);

/**
 * @brief Pause the STA reconnection retry loop before a Wi-Fi scan.
 *
 * Saves the current retry state and prevents the periodic timer callback
 * from calling esp_wifi_connect() while the radio is busy scanning.
 * Must be paired with network_sta_retry_resume_after_scan().
 *
 * @return true  esp_wifi_disconnect() actually aborted a connection;
 *               scan is deferred until WIFI_EVENT_STA_DISCONNECTED
 *               confirms the STA is idle.
 * @return false STA was already idle or connected; scan may start
 *               immediately.
 */
bool network_sta_retry_pause_for_scan(int duration);
/**
 * @brief Resume the STA reconnection retry loop after a Wi-Fi scan completes.
 *
 * Restores the retry state saved by network_sta_retry_pause_for_scan() and
 * restarts the backoff timer so reconnection attempts continue.
 */
void network_sta_retry_resume_after_scan(void);

/**
 * @brief Store the user's persistent default AP state.
 *
 * Runtime fallback AP activation does not call this function and therefore
 * does not alter the user's preference.
 *
 * @param default_active true to keep AP enabled by default, false otherwise.
 */
void network_ap_set_user_default(bool default_active);

/** Reevaluate AP auto-close when an AP-side WebSocket connects or disconnects. */
void network_ap_client_connection_changed(void);

/**
 * @brief Check if the given IP address conflicts with any device on the network.
 *
 * @param target_ip The IP address to check for conflict.
 *
 * @return
 * -  1 if IP conflict detected
 * -  0 if no conflict detected
 * - -1 if invalid parameters (e.g., no network interface or invalid IP)
 */
int network_check_ip_conflict(ip4_addr_t target_ip);

/**
 * @brief Register or unregister the STA IP event handler based on a flag.
 *
 * @param flag If non-zero, registers the handler; if zero, unregisters it.
 */
void network_sta_get_ip_event_register_by_flag(int flag);

/**
 * @brief Register or unregister STA connection event handlers based on a flag.
 *
 * @param flag If non-zero, registers the handlers; if zero, unregisters them.
 */
void network_sta_connect_event_register_by_flag(int flag);

/**
 * @brief Initialize all network components (Wi-Fi, Ethernet, timers, etc.).
 *
 * This function sets up event handlers, initializes Wi-Fi in AP+STA mode,
 * starts Ethernet and Wi-Fi subsystems, configures timers, loads saved network
 * configuration, and starts the captive portal if applicable.
 *
 * It also enables AP mode on first boot for setup purposes.
 */
void network_init(void);

#endif // OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_H

#ifdef __cplusplus
}
#endif