#include <esp_log.h>
#include <esp_system.h>
#include "aes/esp_aes.h"
#include "mbedtls/base64.h"
#include "webserver_config.h"
#include "webserver_method_api.h"
#include "webserver_screen.h"
#include "webserver_config_forget_password.h"
#include "webserver_config_backup.h"
#include "webserver_config_api.h"
#include "network_api.h"
#include "screen_api.h"
#include "screen_night_mode.h"
#include "ha_auth_api.h"
#include "login_session.h"
#include "login_session_api.h"
#include "sys_time.h"
#include "bsp_lcd.h"
#include "mdns_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"


static const char *TAG = "webserver_screen";

static TimerHandle_t screen_lock_timer = NULL;
static bool screen_is_lock = false;

bool screen_is_locked(void)
{
    return screen_is_lock;
}

static void screen_lock_timer_cb(TimerHandle_t xTimer)
{
    bsp_lcd_br_set(0);
    screen_is_lock = true;
    ESP_LOGI(TAG, "Screen locked due to inactivity");
}

void screen_lock_reset_timer(void)
{
    device_info_t *device_info = webserver_config_get_device_info();
    if (device_info->idle_time == 0 || screen_lock_timer == NULL) {
        return;
    }
    if (screen_is_lock) {
        bsp_lcd_br_set(night_mode_get_effective_brightness(device_info->brightness));
        screen_is_lock = false;
        ESP_LOGI(TAG, "Screen unlocked by user activity");
    }
    xTimerReset(screen_lock_timer, 0);
}

esp_err_t screen_set_lock(int idle_time, uint8_t brightness)
{
    // 停止并删除之前设置的计时器
    if (screen_lock_timer != NULL) {
        xTimerStop(screen_lock_timer, 0);
        xTimerDelete(screen_lock_timer, 0);
        screen_lock_timer = NULL;
    }
    // 创建一个新的定时器
    if (idle_time > 0) {
        screen_lock_timer =xTimerCreate(
            "screen_lock",
            pdMS_TO_TICKS(idle_time * 1000),
            pdFALSE,
            (void *) 0,
            screen_lock_timer_cb
        );
        if (screen_lock_timer == NULL) {
            ESP_LOGE(TAG, "xTimerCreate fail");
            return ESP_FAIL;
        }
        xTimerStart(screen_lock_timer, 0);
        screen_is_lock = false;
        ESP_LOGI(TAG, "Screen lock timer started: %d seconds", idle_time);
    } else{
        if (screen_is_lock)
        {
            bsp_lcd_br_set(brightness);
            screen_is_lock = false;
        }
        ESP_LOGI(TAG, "Screen lock disabled (idleTime = 0)");
    }
    return ESP_OK;
}

void screen_lock_init(void)
{
    device_info_t *device_info = webserver_config_get_device_info();
    if (device_info == NULL) {
        ESP_LOGE(TAG, "device_info is NULL");
        return;
    }
    int idle_time = device_info->idle_time;
    if (idle_time == 0) {
        ESP_LOGI(TAG, "Screen lock disabled (idleTime = 0), skip init");
        return;
    }
    // 创建一个新的定时器
    screen_lock_timer =xTimerCreate(
        "screen_lock",
        pdMS_TO_TICKS(idle_time * 1000),
        pdFALSE,
        (void *) 0,
        screen_lock_timer_cb
    );
    if (screen_lock_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create screen lock timer on init");
        return;
    }
    xTimerStart(screen_lock_timer, 0);
    screen_is_lock = false;
    ESP_LOGI(TAG, "Screen lock timer initialized: %d seconds", idle_time);
}