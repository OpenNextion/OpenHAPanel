#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_LAN_MDNS_INCLUDE_MDNS_INIT_H
#define OPEN_NEXTION_COMPONENTS_LAN_MDNS_INCLUDE_MDNS_INIT_H

#include "mdns_api.h"

/**
 * @brief Initialize the mDNS server and related services.
 *
 * This function initializes the mDNS service, loads the configuration from
 * a file, registers event handlers to respond to IP address acquisition
 * on both Wi-Fi and Ethernet interfaces, and starts the mDNS scan task.
 *
 * @param mode Current wireless controller firmware mode.
 * @param baud_rate Current wireless controller UART baud rate.
 *
 * @return int
 *         0 on successful initialization, -1 on failure.
 */
int mdns_server_init(wireless_controller_work_mode_t mode, uint32_t baud_rate);

/**
 * @brief Pause or resume periodic LAN mDNS discovery scans.
 *
 * The local mDNS responder remains active while discovery is paused. This is
 * used during memory-intensive TLS operations such as OTA so a new query does
 * not compete for scarce internal RAM. An already-running query is allowed to
 * finish safely.
 */
void mdns_scan_set_paused(bool paused);

#endif // OPEN_NEXTION_COMPONENTS_LAN_MDNS_INCLUDE_MDNS_INIT_H

#ifdef __cplusplus
}
#endif
