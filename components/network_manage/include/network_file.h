#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_FILE_H
#define OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_FILE_H

#include "esp_err.h"
#include <stdbool.h>
#include "network.h"

/**
 * @brief Save network configuration to persistent JSON file.
 *
 * Serializes the given NetworkState structure and writes it to storage.
 *
 * @param network_config The network configuration to save.
 *
 * @return
 * - ESP_OK on success.
 * - ESP_FAIL on failure.
 */
int network_save_config_to_file(NetworkState network_config);

/**
 * @brief Load network configuration from file and apply network settings.
 *
 * Loads stored network settings, applies default config if loading fails,
 * manages Wi-Fi station and Ethernet DHCP connections, configures AP mode,
 * and syncs MQTT network status.
 */
void network_load_and_apply_config(void);

#endif // OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_FILE_H

#ifdef __cplusplus
}
#endif
