#include "screen_theme.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "ui_preferences.h"

ESP_EVENT_DEFINE_BASE(SCREEN_THEME_EVENT);

static const char *TAG = "screen_theme";

static SemaphoreHandle_t s_theme_lock;
static bool s_initialized;
static screen_theme_mode_t s_current_theme = SCREEN_THEME_DARK;

static esp_err_t screen_theme_post_changed_event(
    screen_theme_mode_t old_theme,
    screen_theme_mode_t new_theme)
{
    const screen_theme_changed_event_t event = {
        .old_theme = old_theme,
        .new_theme = new_theme,
    };
    esp_err_t err = esp_event_post(SCREEN_THEME_EVENT,
                                   SCREEN_EVENT_THEME_CHANGED,
                                   &event,
                                   sizeof(event),
                                   pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to publish theme changed event: %s",
                 esp_err_to_name(err));
    }
    return err;
}

bool screen_theme_is_valid(screen_theme_mode_t mode)
{
    return mode == SCREEN_THEME_DARK || mode == SCREEN_THEME_LIGHT;
}

const char *screen_theme_mode_to_string(screen_theme_mode_t mode)
{
    switch (mode) {
        case SCREEN_THEME_DARK:
            return "dark";
        case SCREEN_THEME_LIGHT:
            return "light";
        default:
            return NULL;
    }
}

esp_err_t screen_theme_mode_from_string(const char *value,
                                        screen_theme_mode_t *mode)
{
    if (!value || !mode) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(value, "dark") == 0) {
        *mode = SCREEN_THEME_DARK;
        return ESP_OK;
    }
    if (strcmp(value, "light") == 0) {
        *mode = SCREEN_THEME_LIGHT;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t screen_theme_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_theme_lock = xSemaphoreCreateMutex();
    if (!s_theme_lock) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t stored_theme = (uint8_t)SCREEN_THEME_DARK;
    esp_err_t read_err = ui_preferences_get_screen_theme(&stored_theme);

    if (read_err == ESP_ERR_NVS_NOT_FOUND) {
        read_err = ESP_OK;
    }
    else if (read_err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to read persisted screen theme: %s; using dark",
                 esp_err_to_name(read_err));
    }
    else if (!screen_theme_is_valid((screen_theme_mode_t)stored_theme)) {
        ESP_LOGW(TAG,
                 "Unsupported persisted screen theme value: %u; using dark",
                 (unsigned)stored_theme);
        stored_theme = (uint8_t)SCREEN_THEME_DARK;
    }

    s_current_theme = (screen_theme_mode_t)stored_theme;
    s_initialized = true;

    ESP_LOGI(TAG,
             "Initialized: theme=%s",
             screen_theme_mode_to_string(s_current_theme));

    /* Keep the service usable with the safe default even when NVS could not
     * be read, while still reporting that storage failure to the caller. */
    return read_err;
}

esp_err_t screen_theme_set(screen_theme_mode_t mode)
{
    return screen_theme_set_with_changed(mode, NULL);
}

esp_err_t screen_theme_set_with_changed(screen_theme_mode_t mode,
                                        bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (!screen_theme_is_valid(mode)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_theme_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_theme_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_current_theme == mode) {
        xSemaphoreGive(s_theme_lock);
        return ESP_OK;
    }

    const screen_theme_mode_t old_theme = s_current_theme;
    esp_err_t err = ui_preferences_set_screen_theme((uint8_t)mode);
    if (err == ESP_OK) {
        s_current_theme = mode;
        if (changed) {
            *changed = true;
        }
    }
    else {
        ESP_LOGE(TAG,
                 "Failed to persist screen theme %s: %s",
                 screen_theme_mode_to_string(mode),
                 esp_err_to_name(err));
    }

    xSemaphoreGive(s_theme_lock);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG,
             "Theme updated: %s -> %s",
             screen_theme_mode_to_string(old_theme),
             screen_theme_mode_to_string(mode));
    return screen_theme_post_changed_event(old_theme, mode);
}

screen_theme_mode_t screen_theme_get(void)
{
    if (!s_initialized || !s_theme_lock) {
        return SCREEN_THEME_DARK;
    }
    if (xSemaphoreTake(s_theme_lock, portMAX_DELAY) != pdTRUE) {
        return SCREEN_THEME_DARK;
    }

    screen_theme_mode_t mode = s_current_theme;
    xSemaphoreGive(s_theme_lock);
    return mode;
}
