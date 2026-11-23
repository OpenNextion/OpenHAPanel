/* SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "key.h"

#include "button_gpio.h"
#include "iot_button.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define KEY_FACTORY_RESET_HOLD_TIME_MS 10000U
#define KEY_LONG_PRESS_EVENT_TIME_MS    10050U
#define KEY_MULTI_CLICK_INTERVAL_MS      500U

static const char *TAG = "key";

ESP_EVENT_DEFINE_BASE(KEY_EVENT);

static button_handle_t s_button;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static struct {
    int64_t pressed_at_us;
    bool pressed;
    bool factory_reset_triggered;
    bool wait_for_release;
    bool ignore_current_cycle;
} s_state;

static void key_clear_state_locked(void)
{
    s_state.pressed_at_us = 0;
    s_state.pressed = false;
    s_state.factory_reset_triggered = false;
}

static esp_err_t key_post_event(key_event_id_t event_id,
                                const key_factory_reset_event_data_t *event_data)
{
    size_t event_data_size = event_data ? sizeof(*event_data) : 0;
    return esp_event_post(KEY_EVENT,
                          event_id,
                          event_data,
                          event_data_size,
                          0);
}

static void key_press_down_cb(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    int64_t now_us = esp_timer_get_time();
    taskENTER_CRITICAL(&s_state_lock);
    if (!s_state.wait_for_release && !s_state.ignore_current_cycle) {
        s_state.pressed_at_us = now_us;
        s_state.pressed = true;
        s_state.factory_reset_triggered = false;
    }
    taskEXIT_CRITICAL(&s_state_lock);
}

static void key_press_up_cb(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    taskENTER_CRITICAL(&s_state_lock);
    key_clear_state_locked();
    s_state.wait_for_release = false;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void key_single_click_cb(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    taskENTER_CRITICAL(&s_state_lock);
    bool ignore = s_state.ignore_current_cycle;
    taskEXIT_CRITICAL(&s_state_lock);
    if (!ignore) {
        key_post_event(KEY_EVENT_SINGLE_CLICK, NULL);
    }
}

static void key_double_click_cb(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    taskENTER_CRITICAL(&s_state_lock);
    bool ignore = s_state.ignore_current_cycle;
    taskEXIT_CRITICAL(&s_state_lock);
    if (!ignore) {
        key_post_event(KEY_EVENT_DOUBLE_CLICK, NULL);
    }
}

static void key_press_end_cb(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    taskENTER_CRITICAL(&s_state_lock);
    key_clear_state_locked();
    s_state.ignore_current_cycle = false;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void key_long_press_start_cb(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;

    int64_t now_us = esp_timer_get_time();
    int gpio_level = gpio_get_level(KEY_BUTTON_GPIO);
    bool triggered = false;
    bool pressed = false;
    int64_t pressed_at_us = 0;

    taskENTER_CRITICAL(&s_state_lock);
    pressed = s_state.pressed;
    pressed_at_us = s_state.pressed_at_us;
    if (pressed &&
        !s_state.ignore_current_cycle &&
        !s_state.factory_reset_triggered &&
        pressed_at_us > 0 &&
        now_us - pressed_at_us >= (int64_t)KEY_FACTORY_RESET_HOLD_TIME_MS * 1000 &&
        gpio_level == KEY_BUTTON_ACTIVE_LEVEL) {
        s_state.factory_reset_triggered = true;
        triggered = true;
    }
    taskEXIT_CRITICAL(&s_state_lock);

    if (!triggered) {
        return;
    }

    key_factory_reset_event_data_t event_data = {
        .pressed_duration_ms = (uint32_t)((now_us - pressed_at_us) / 1000),
        .gpio_level = gpio_level,
    };
    if (key_post_event(KEY_EVENT_FACTORY_RESET_TRIGGERED, &event_data) != ESP_OK) {
        taskENTER_CRITICAL(&s_state_lock);
        s_state.factory_reset_triggered = false;
        taskEXIT_CRITICAL(&s_state_lock);
    }
}

static esp_err_t key_register_callbacks(button_handle_t button)
{
    esp_err_t err = iot_button_register_cb(button, BUTTON_PRESS_DOWN, NULL, key_press_down_cb, NULL);
    if (err != ESP_OK) return err;

    err = iot_button_register_cb(button, BUTTON_PRESS_UP, NULL, key_press_up_cb, NULL);
    if (err != ESP_OK) return err;

    err = iot_button_register_cb(button, BUTTON_SINGLE_CLICK, NULL, key_single_click_cb, NULL);
    if (err != ESP_OK) return err;

    err = iot_button_register_cb(button, BUTTON_DOUBLE_CLICK, NULL, key_double_click_cb, NULL);
    if (err != ESP_OK) return err;

    err = iot_button_register_cb(button, BUTTON_PRESS_END, NULL, key_press_end_cb, NULL);
    if (err != ESP_OK) return err;

    button_event_args_t long_press_args = {
        /* The official component and our monotonic safety clock are sampled at
         * slightly different instants. A small margin guarantees that the
         * independent safety check has observed the full ten seconds. */
        .long_press.press_time = KEY_LONG_PRESS_EVENT_TIME_MS,
    };
    err = iot_button_register_cb(button,
                                 BUTTON_LONG_PRESS_START,
                                 &long_press_args,
                                 key_long_press_start_cb,
                                 NULL);
    return err;
}

esp_err_t key_init(void)
{
    if (s_button) return ESP_ERR_INVALID_STATE;

    button_config_t button_config = {
        .long_press_time = KEY_LONG_PRESS_EVENT_TIME_MS,
        /* The previous implementation allowed about 20 x 25 ms between
         * clicks. In the official component this field is the repeat/click
         * decision interval, not the maximum physical press duration. */
        .short_press_time = KEY_MULTI_CLICK_INTERVAL_MS,
    };
    button_gpio_config_t gpio_config = {
        .gpio_num = KEY_BUTTON_GPIO,
        .active_level = KEY_BUTTON_ACTIVE_LEVEL,
        .enable_power_save = false,
        .disable_pull = false,
    };

    button_handle_t button = NULL;
    esp_err_t err = iot_button_new_gpio_device(&button_config, &gpio_config, &button);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create GPIO button: %s", esp_err_to_name(err));
        return err;
    }

    bool wait_for_release =
        gpio_get_level(KEY_BUTTON_GPIO) == KEY_BUTTON_ACTIVE_LEVEL;
    taskENTER_CRITICAL(&s_state_lock);
    key_clear_state_locked();
    s_state.wait_for_release = wait_for_release;
    s_state.ignore_current_cycle = wait_for_release;
    taskEXIT_CRITICAL(&s_state_lock);

    err = key_register_callbacks(button);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register button callbacks: %s", esp_err_to_name(err));
        iot_button_delete(button);
        taskENTER_CRITICAL(&s_state_lock);
        key_clear_state_locked();
        s_state.wait_for_release = false;
        s_state.ignore_current_cycle = false;
        taskEXIT_CRITICAL(&s_state_lock);
        return err;
    }

    /* The button may have been released while callbacks were being installed.
     * Do not leave the startup gate latched if the raw pin is already inactive. */
    if (wait_for_release &&
        gpio_get_level(KEY_BUTTON_GPIO) != KEY_BUTTON_ACTIVE_LEVEL) {
        wait_for_release = false;
        taskENTER_CRITICAL(&s_state_lock);
        s_state.wait_for_release = false;
        s_state.ignore_current_cycle = false;
        taskEXIT_CRITICAL(&s_state_lock);
    }

    taskENTER_CRITICAL(&s_state_lock);
    s_button = button;
    taskEXIT_CRITICAL(&s_state_lock);

    ESP_LOGI(TAG,
             "BOOT button initialized: gpio=%d active_level=%d factory_reset_hold=%u ms",
             KEY_BUTTON_GPIO,
             KEY_BUTTON_ACTIVE_LEVEL,
             KEY_FACTORY_RESET_HOLD_TIME_MS);
    if (wait_for_release) {
        ESP_LOGW(TAG,
                 "BOOT button is held during initialization; waiting for release before accepting presses");
    }
    return ESP_OK;
}

esp_err_t key_de_init(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    button_handle_t button = s_button;
    s_button = NULL;
    key_clear_state_locked();
    s_state.wait_for_release = false;
    s_state.ignore_current_cycle = false;
    taskEXIT_CRITICAL(&s_state_lock);

    if (!button) return ESP_ERR_INVALID_STATE;

    esp_err_t err = iot_button_delete(button);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete button: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "BOOT button deinitialized");
    return ESP_OK;
}
