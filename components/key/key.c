/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "key.h"
#include "sdkconfig.h"

enum
{
    KEY_STATE_NONE = 0,        /* initially idle stateless */
    KEY_STATE_DEBOUNCING,      /* key debiting */
    PRESS_DOWN_CHECK,          /* detection by pressing the button */
    PRESS_UP_CHECK,            /* button pop up detection */
    PRESS_REPEAT_DOWN_CHECK,   /* press the button double click to detect */
    PRESS_LONG_PRESS_UP_CHECK, /* Press and hold the button for 10s to bounce up and detect */
};

/**
 * @brief Structs to record individual key parameters
 *
 */
typedef struct button_dev_t
{
    uint32_t ticks;
    uint8_t repeat;
    uint8_t state : 3;
    uint8_t last_level : 1;
    uint8_t curr_level : 1;
    int count[2];
} button_dev_t;

#define TICKS_INTERVAL CONFIG_BUTTON_PERIOD_TIME_MS                      /* ticks */
#define DEBOUNCE_TICKS CONFIG_BUTTON_DEBOUNCE_TICKS                      /* debounce ticks */
#define SHORT_TICKS (CONFIG_BUTTON_SHORT_PRESS_TIME_MS / TICKS_INTERVAL) /* click timeout duration */
#define REPEAT_TIMES (20) /* the duration of the double click interval timeout */

#define LONG_TICKS (CONFIG_BUTTON_LONG_PRESS_TIME_MS / TICKS_INTERVAL) /* press and hold the button for a long time */

static const char *TAG = "key";

/* define the event group */
ESP_EVENT_DEFINE_BASE(KEY_EVENT);

/* define button handle */
static button_dev_t g_button_handle = {0};

/* key timer handle */
static esp_timer_handle_t g_button_timer_handle = NULL;
/* whether the key timer runs the flag bit */
static bool g_is_timer_running = false;

/**
 * @brief Button scanning callback function triggered periodically by esp_timer
 *
 * This function implements a state machine for debounced GPIO button input. It is called
 * at a fixed interval (defined by TICKS_INTERVAL) to poll the current state of the button
 * and determine whether the user has performed a single-click, double-click, or long-press.
 *
 * The button states are transitioned based on GPIO level, debounce timing, and press duration.
 *
 * Events such as single click, double click, and long press start are posted to the
 * specified event loop (device_loop) for application handling.
 *
 * @param[in] args Unused (reserved for esp_timer compatibility)
 */
static void button_cb(void *args)
{
    /* gets the current level status of the key */
    g_button_handle.curr_level = gpio_get_level(BUTTON_IO_NUM);

    /** State machine */
    switch (g_button_handle.state) {
    case KEY_STATE_NONE:
        if (g_button_handle.curr_level != g_button_handle.last_level) {
            g_button_handle.ticks = 0;
            g_button_handle.repeat = 0;
            g_button_handle.state = KEY_STATE_DEBOUNCING;
        }
        break;

    case KEY_STATE_DEBOUNCING:
        g_button_handle.ticks++;
        /* key debiting */
        if (g_button_handle.curr_level != g_button_handle.last_level) {
            if (g_button_handle.ticks >= DEBOUNCE_TICKS) {
                g_button_handle.last_level = g_button_handle.curr_level;
                g_button_handle.state = PRESS_DOWN_CHECK;
            }
        }
        else {
            g_button_handle.state = KEY_STATE_NONE;
        }
        break;

    case PRESS_DOWN_CHECK:
        if (g_button_handle.curr_level == BUTTON_ACTIVE_LEVEL) {
            g_button_handle.state = PRESS_UP_CHECK;
        }
        else {
            g_button_handle.state = KEY_STATE_NONE;
        }
        g_button_handle.ticks = 0;
        break;

    case PRESS_UP_CHECK:
        g_button_handle.ticks++;
        /* determine whether to press and hold it */
        if (g_button_handle.ticks >= LONG_TICKS) {
            g_button_handle.state = PRESS_LONG_PRESS_UP_CHECK;
            /* send a long press for 10s event */
            esp_event_post(KEY_EVENT, BUTTON_LONG_PRESS_START, NULL, 0, portMAX_DELAY);
            break;
        }

        if (g_button_handle.curr_level != BUTTON_ACTIVE_LEVEL) {
            g_button_handle.last_level = g_button_handle.curr_level;
            /* determine whether the click timed out */
            if (g_button_handle.ticks < SHORT_TICKS) {
                g_button_handle.ticks = 0;
                g_button_handle.repeat++;
                g_button_handle.state = PRESS_REPEAT_DOWN_CHECK;
            }
            else /* click timeout to cancel the button*/
            {
                g_button_handle.state = KEY_STATE_NONE;
            }
        }
        break;

    case PRESS_REPEAT_DOWN_CHECK:
        g_button_handle.ticks++;
        /* Determine whether there are still keys pressed before the double-click timeout */
        if (g_button_handle.ticks > REPEAT_TIMES) {
            if (g_button_handle.repeat == 1) {
                esp_event_post(KEY_EVENT, BUTTON_SINGLE_CLICK, NULL, 0, portMAX_DELAY);
            }
            else if (g_button_handle.repeat == 2) {
                esp_event_post(KEY_EVENT, BUTTON_DOUBLE_CLICK, NULL, 0, portMAX_DELAY);
            }
            else {
            }
            g_button_handle.state = KEY_STATE_NONE;
        }
        else {
            if (g_button_handle.curr_level != g_button_handle.last_level) {
                if (g_button_handle.curr_level == BUTTON_ACTIVE_LEVEL) {
                    g_button_handle.state = KEY_STATE_DEBOUNCING;
                }
            }
        }
        break;

    case PRESS_LONG_PRESS_UP_CHECK:
        if (g_button_handle.curr_level != g_button_handle.last_level) {
            /* release */
            if (g_button_handle.curr_level != BUTTON_ACTIVE_LEVEL) {
                g_button_handle.state = KEY_STATE_NONE;
            }
        }
        break;
    default:
        ESP_LOGI(TAG, "state error");
    }
}

/**
 * @brief Initialize the button driver and start the polling timer
 *
 * This function sets up the internal button state and creates a periodic timer using
 * the ESP timer service to poll the GPIO pin. The button logic is implemented through
 * a software state machine, and events will be dispatched based on user interactions
 * such as short press, double click, and long press.
 *
 * This function should be called once during initialization.
 *
 * @return
 *      - ESP_OK: Initialization successful
 *      - Other error codes are not returned in current implementation
 */
esp_err_t key_iot_button_create(void)
{
    g_button_handle.state = KEY_STATE_NONE;
    g_button_handle.last_level = gpio_get_level(BUTTON_IO_NUM);

    if (!g_button_timer_handle) {
        esp_timer_create_args_t button_timer = {0};
        button_timer.arg = NULL;
        button_timer.callback = button_cb;
        button_timer.dispatch_method = ESP_TIMER_TASK;
        button_timer.name = "button_timer";
        esp_timer_create(&button_timer, &g_button_timer_handle);
    }

    if (!g_is_timer_running) {
        esp_timer_start_periodic(g_button_timer_handle, TICKS_INTERVAL * 1000U);
        g_is_timer_running = true;
    }
    return ESP_OK;
}

/**
 * @brief Stop the button polling timer and release resources
 *
 * This function stops the periodic polling timer used to detect button states,
 * and deletes the timer to free system resources. It should be called when the
 * button is no longer needed or before system shutdown to avoid memory leaks.
 *
 * @return
 *      - ESP_OK: De-initialization successful
 *      - Other error codes are not returned in current implementation
 */
static esp_err_t iot_button_delete(void)
{
    esp_timer_stop(g_button_timer_handle);
    esp_timer_delete(g_button_timer_handle);
    g_button_timer_handle = NULL;
    g_is_timer_running = false;
    return ESP_OK;
}

/**
 * @brief Initialize button hardware and logic
 *
 * This function configures the GPIO pin used for button input and starts the
 * internal software button handler by creating a periodic timer.
 * It sets up pull-up configuration and disables GPIO interrupts, relying instead
 * on polling for state changes.
 */
void key_init(void)
{
    gpio_config_t gpio_conf = {
        .pin_bit_mask = 1ULL << BUTTON_IO_NUM,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_conf);

    key_iot_button_create();
}

/**
 * @brief De-initialize button logic and clean up resources
 *
 * This function stops the internal polling timer and deletes it. It also resets the
 * GPIO configuration to a known state. Use this function before shutdown or when the
 * button is no longer needed to free resources.
 */
void key_de_init(void)
{
    esp_err_t ret = iot_button_delete();
    if (ESP_OK == ret) {
        ESP_LOGI(TAG, "Button deleted successfully");
    }
    else {
        ESP_LOGE(TAG, "Button delete failed");
    }
    gpio_config_t gpio_conf = {
        .pin_bit_mask = 1ULL << BUTTON_IO_NUM,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_conf);
}