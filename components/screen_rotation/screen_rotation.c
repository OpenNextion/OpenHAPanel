#include "screen_rotation.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "screen.h"
#include "ui_preferences.h"

#define SCREEN_ROTATION_DEFAULT_QUARTER_TURNS 0U
#define SCREEN_ROTATION_90_QUARTER_TURNS      1U

static const char *TAG = "screen_rotation";

static SemaphoreHandle_t s_rotation_lock;
static bool s_initialized;
static screen_rotation_transition_prepare_cb_t s_transition_prepare_cb;
static screen_rotation_transition_complete_cb_t s_transition_complete_cb;
static void *s_transition_user_data;

static bool screen_rotation_is_supported(bsp_lcd_rotation_t rotation)
{
    return rotation == BSP_LCD_ROTATION_0 || rotation == BSP_LCD_ROTATION_90;
}

static uint8_t screen_rotation_to_quarter_turns(bsp_lcd_rotation_t rotation)
{
    return rotation == BSP_LCD_ROTATION_90
               ? SCREEN_ROTATION_90_QUARTER_TURNS
               : SCREEN_ROTATION_DEFAULT_QUARTER_TURNS;
}

static bsp_lcd_orientation_t screen_rotation_to_orientation(
    bsp_lcd_rotation_t rotation)
{
    return rotation == BSP_LCD_ROTATION_90 ||
                   rotation == BSP_LCD_ROTATION_270
               ? BSP_LCD_ORIENTATION_LANDSCAPE
               : BSP_LCD_ORIENTATION_PORTRAIT;
}

static esp_err_t screen_rotation_post_changed_event(
    const screen_orientation_changed_event_t *event)
{
    esp_err_t err = esp_event_post(SCREEN_EVENT,
                                   SCREEN_EVENT_ORIENTATION_CHANGED,
                                   event,
                                   sizeof(*event),
                                   pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to publish orientation changed event: %s",
                 esp_err_to_name(err));
    }
    return err;
}

esp_err_t screen_rotation_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_rotation_lock = xSemaphoreCreateMutex();
    if (!s_rotation_lock) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t stored_quarter_turns = SCREEN_ROTATION_DEFAULT_QUARTER_TURNS;
    esp_err_t read_err = ui_preferences_get_screen_rotation(&stored_quarter_turns);
    bsp_lcd_rotation_t rotation = BSP_LCD_ROTATION_0;

    if (read_err == ESP_ERR_NVS_NOT_FOUND) {
        read_err = ESP_OK;
    }
    else if (read_err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to read persisted screen rotation: %s; using 0 degrees",
                 esp_err_to_name(read_err));
    }
    else if (stored_quarter_turns == SCREEN_ROTATION_90_QUARTER_TURNS) {
        rotation = BSP_LCD_ROTATION_90;
    }
    else if (stored_quarter_turns != SCREEN_ROTATION_DEFAULT_QUARTER_TURNS) {
        ESP_LOGW(TAG,
                 "Unsupported persisted screen rotation value: %u; using 0 degrees",
                 (unsigned)stored_quarter_turns);
    }

    esp_err_t apply_err = bsp_lcd_set_rotation(rotation);
    if (apply_err != ESP_OK) {
        vSemaphoreDelete(s_rotation_lock);
        s_rotation_lock = NULL;
        return apply_err;
    }

    s_initialized = true;
    ESP_LOGI(TAG,
             "Initialized: rotation=%d orientation=%s",
             (int)bsp_lcd_get_rotation(),
             bsp_lcd_is_landscape() ? "landscape" : "portrait");

    /* A storage read failure is reported to the caller, but the service stays
     * usable with the safe default rotation that was applied above. */
    return read_err;
}

void screen_rotation_set_transition_callbacks(
    screen_rotation_transition_prepare_cb_t prepare_cb,
    screen_rotation_transition_complete_cb_t complete_cb,
    void *user_data)
{
    s_transition_prepare_cb = prepare_cb;
    s_transition_complete_cb = complete_cb;
    s_transition_user_data = user_data;
}

esp_err_t screen_rotation_set(bsp_lcd_rotation_t rotation)
{
    if (!screen_rotation_is_supported(rotation)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_rotation_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_rotation_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    const bsp_lcd_rotation_t old_rotation = bsp_lcd_get_rotation();
    if (old_rotation == rotation) {
        xSemaphoreGive(s_rotation_lock);
        return ESP_OK;
    }

    screen_rotation_transition_prepare_cb_t prepare_cb =
        s_transition_prepare_cb;
    screen_rotation_transition_complete_cb_t complete_cb =
        s_transition_complete_cb;
    void *transition_user_data = s_transition_user_data;

    if (prepare_cb) {
        esp_err_t prepare_err = prepare_cb(transition_user_data);
        if (prepare_err != ESP_OK) {
            xSemaphoreGive(s_rotation_lock);
            if (complete_cb) {
                complete_cb(prepare_err, transition_user_data);
            }
            ESP_LOGE(TAG,
                     "Rotation transition preparation failed: %s",
                     esp_err_to_name(prepare_err));
            return prepare_err;
        }
    }

    screen_orientation_changed_event_t event = {
        .old_rotation = old_rotation,
        .new_rotation = rotation,
        .old_orientation = screen_rotation_to_orientation(old_rotation),
        .new_orientation = screen_rotation_to_orientation(rotation),
    };

    esp_err_t err = bsp_lcd_set_rotation(rotation);
    if (err == ESP_OK) {
        err = ui_preferences_set_screen_rotation(
            screen_rotation_to_quarter_turns(rotation));
    }
    if (err != ESP_OK && bsp_lcd_get_rotation() != old_rotation) {
        esp_err_t rollback_err = bsp_lcd_set_rotation(old_rotation);
        if (rollback_err != ESP_OK) {
            ESP_LOGE(TAG,
                     "Failed to restore rotation %d after transition error: %s",
                     (int)old_rotation,
                     esp_err_to_name(rollback_err));
        }
        else {
            esp_err_t persist_rollback_err = ui_preferences_set_screen_rotation(
                screen_rotation_to_quarter_turns(old_rotation));
            if (persist_rollback_err != ESP_OK) {
                ESP_LOGE(TAG,
                         "Failed to restore persisted rotation %d: %s",
                         (int)old_rotation,
                         esp_err_to_name(persist_rollback_err));
            }
        }
    }

    bool publish_event = false;
    if (err == ESP_OK) {
        esp_err_t resolution_err = bsp_lcd_get_logical_resolution(&event.width,
                                                                   &event.height);
        if (resolution_err == ESP_OK) {
            publish_event = true;
        }
        else {
            ESP_LOGW(TAG,
                     "Rotation persisted but logical resolution is unavailable: %s",
                     esp_err_to_name(resolution_err));
        }
        ESP_LOGI(TAG,
                 "Rotation updated: rotation=%d orientation=%s",
                 (int)rotation,
                 bsp_lcd_is_landscape() ? "landscape" : "portrait");
    }
    else {
        ESP_LOGE(TAG,
                 "Failed to apply and persist rotation %d: %s",
                 (int)rotation,
                 esp_err_to_name(err));
    }

    esp_err_t transition_result = err;
    if (publish_event) {
        esp_err_t event_err = screen_rotation_post_changed_event(&event);
        if (transition_result == ESP_OK) {
            transition_result = event_err;
        }
    }
    else if (transition_result == ESP_OK) {
        transition_result = ESP_ERR_INVALID_STATE;
    }

    xSemaphoreGive(s_rotation_lock);
    if (complete_cb) {
        complete_cb(transition_result, transition_user_data);
    }
    return err;
}

bsp_lcd_rotation_t screen_rotation_get(void)
{
    return bsp_lcd_get_rotation();
}
