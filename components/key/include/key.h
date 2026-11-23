/* SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef OPEN_NEXTION_COMPONENTS_KEY_INCLUDE_KEY_H
#define OPEN_NEXTION_COMPONENTS_KEY_INCLUDE_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(KEY_EVENT);

#define KEY_BUTTON_ACTIVE_LEVEL 0
#define KEY_BUTTON_GPIO         GPIO_NUM_0

typedef enum {
    KEY_EVENT_SINGLE_CLICK,
    KEY_EVENT_DOUBLE_CLICK,
    KEY_EVENT_FACTORY_RESET_TRIGGERED,
    KEY_EVENT_MAX,
} key_event_id_t;

typedef struct {
    uint32_t pressed_duration_ms;
    int gpio_level;
} key_factory_reset_event_data_t;

/** Initialize the BOOT button using the official espressif/button component. */
esp_err_t key_init(void);

/** Delete the official button handle and release its resources. */
esp_err_t key_de_init(void);

#ifdef __cplusplus
}
#endif

#endif /* OPEN_NEXTION_COMPONENTS_KEY_INCLUDE_KEY_H */
