#pragma once

#include <stdbool.h>

#include "esp_event.h"
#include "firmware_manifest.h"
#include "firmware_update_types.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(FIRMWARE_UPDATE_EVENT);

typedef enum {
    FIRMWARE_UPDATE_EVENT_STATUS_CHANGED = 0,
} firmware_update_event_id_t;

/**
 * @brief Initialize the firmware update state manager.
 *
 * The default ESP event loop must exist before this function is called.
 * Calling it more than once is safe.
 */
esp_err_t firmware_update_init(void);

/**
 * @brief Start a non-blocking Manifest download and update check.
 *
 * Only one OTA operation may run at a time. Results are reported through
 * FIRMWARE_UPDATE_EVENT_STATUS_CHANGED and firmware_update_get_status().
 * If firmware is already staged, the staged version is used as the comparison
 * baseline and remains available when no newer release is found or the check
 * fails.
 */
esp_err_t firmware_update_check_async(void);

/**
 * @brief Download and stage the last validated available firmware.
 *
 * This call is non-blocking and is only valid while the state is AVAILABLE.
 * A successful operation stops in STAGED. It does not change the boot
 * partition, so an unrelated restart continues to boot the current image.
 */
esp_err_t firmware_update_install_async(void);

/**
 * @brief Download and stage the available firmware as an automatic update.
 *
 * This has the same safety behavior as firmware_update_install_async(), but
 * persists the automatic origin so the UI can show the staged-update prompt.
 */
esp_err_t firmware_update_install_automatic_async(void);

/**
 * @brief Validate and select the staged image as the next boot image.
 *
 * This call starts a background validation task and is only valid in STAGED.
 * On success the state becomes READY_TO_REBOOT; the caller decides when to
 * restart the device.
 */
esp_err_t firmware_update_activate_staged(void);

/**
 * @brief Read the persistent staged-image metadata.
 *
 * Returns ESP_ERR_NOT_FOUND when no complete staged image is available.
 */
esp_err_t firmware_update_get_staged_info(firmware_update_staged_info_t *info);

/**
 * @brief Confirm a pending image after the application self-test succeeds.
 *
 * This is a no-op on ordinary boots and is safe to call once at the end of
 * essential application initialization.
 */
esp_err_t firmware_update_confirm_boot(void);

/**
 * @brief Mark a pending image invalid and immediately roll back.
 *
 * Returns ESP_ERR_INVALID_STATE when the running image is not pending
 * verification. A successful call does not return because the device reboots.
 */
esp_err_t firmware_update_rollback_and_reboot(esp_err_t error);

/**
 * @brief Restart into an image successfully installed by OTA.
 *
 * This non-blocking call is only valid in READY_TO_REBOOT.
 */
esp_err_t firmware_update_reboot(void);

/**
 * @brief Copy the current firmware update status into caller-owned memory.
 */
esp_err_t firmware_update_get_status(firmware_update_status_t *status);

/**
 * @brief Read persistent information about the most recent OTA operation.
 */
esp_err_t firmware_update_get_history(firmware_update_history_t *history);

/**
 * @brief Clear the persistent rollback notification after the user sees it.
 */
esp_err_t firmware_update_acknowledge_rollback(void);

/**
 * Prevent new OTA work from persisting and erase staged-image metadata before
 * a factory reset. This does not erase OTA partitions or otadata.
 */
esp_err_t firmware_update_prepare_factory_reset(void);

/**
 * @brief Copy the last validated newer Manifest into caller-owned memory.
 *
 * Returns ESP_ERR_NOT_FOUND unless the current state is AVAILABLE and a
 * validated candidate is held in memory.
 */
esp_err_t firmware_update_get_available_manifest(firmware_manifest_t *manifest);

/**
 * @brief Return true while an update operation owns the OTA workflow.
 */
bool firmware_update_is_busy(void);

/**
 * @brief Return a stable log/UI name for a firmware update state.
 */
const char *firmware_update_state_to_string(firmware_update_state_t state);

#ifdef __cplusplus
}
#endif
