/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifdef __cplusplus
extern "C" {

#endif

#ifndef OPEN_NEXTION_COMPONENTS_KEY_INCLUDE_KEY_H
#define OPEN_NEXTION_COMPONENTS_KEY_INCLUDE_KEY_H

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(KEY_EVENT);

#define BUTTON_ACTIVE_LEVEL (0)
#define BUTTON_IO_NUM GPIO_NUM_0

/**
 * @brief Button events
 *
 */
typedef enum
{
	BUTTON_SINGLE_CLICK,
	BUTTON_DOUBLE_CLICK,
	BUTTON_LONG_PRESS_START,
	BUTTON_EVENT_MAX,
} button_event_e;

/**
 * @brief Initialize the button input and software handling
 *
 * This function must be called before using the button. It sets up GPIO
 * and software logic to detect button presses and generate events.
 */
void key_init(void);

/**
 * @brief De-initialize button input and stop background processing
 *
 * Stops the internal timer and releases GPIO resources associated with the button.
 * Should be called when the button is no longer needed.
 */
void key_de_init(void);

#endif // OPEN_NEXTION_COMPONENTS_KEY_INCLUDE_KEY_H

#ifdef __cplusplus
}
#endif
