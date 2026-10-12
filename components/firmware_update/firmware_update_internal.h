#pragma once

#include "firmware_update_types.h"

/**
 * @brief Move the OTA state machine to a new state and publish a snapshot.
 *
 * This interface is private to the firmware_update component. A NULL target
 * version preserves the current value; entering IDLE clears it.
 */
esp_err_t firmware_update_transition(firmware_update_state_t state,
                                     uint8_t progress,
                                     esp_err_t error,
                                     const char *target_version);
