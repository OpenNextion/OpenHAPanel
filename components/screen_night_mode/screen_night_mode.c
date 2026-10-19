#include "screen_night_mode.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "cJSON.h"
#include "cjson_helpers.h"
#include "littlefs.h"
#include "bsp_lcd.h"
#include "sys_time_api.h"
#include "webserver_config_api.h"
#include "webserver_config.h"
#include "webserver_screen.h"

#define NIGHT_MODE_CHECK_PERIOD_MS (30000)
#define NIGHT_MODE_CONFIG_FILE "night_mode.json"
#define NIGHT_MODE_TIME_STR_LEN 10

static const char *TAG = "night_mode";

extern night_mode_config_t s_night_mode_config;
static TimerHandle_t s_night_mode_timer = NULL;
static bool s_in_night_period;  /** @brief Whether current time is in night period */

static bool night_mode_parse_time(const char *time_str, int *hour, int *min)
{
    if(!time_str || strlen(time_str) < 5) {
        return false;
    }
    int h, m;
    if(sscanf(time_str, "%d:%d", &h, &m) != 2) {
        return false;
    }
    *hour = h;
    *min = m;
    return true;
}

static bool night_mode_is_in_time_range(const char *from, const char *to)
{
    if (from == NULL || to == NULL) {
        return false;
    }
    int from_h, from_m ,to_h, to_m;
    if(!night_mode_parse_time(from, &from_h, &from_m) || 
        !night_mode_parse_time(to, &to_h, &to_m)) {
        return false;
    }
    struct tm *current_tm = sys_time_get_local_time();
    if(!current_tm) {
        return false;
    }
    int current_min = current_tm->tm_hour * 60 + current_tm->tm_min;
    int from_min = from_h * 60 + from_m;
    int to_min = to_h * 60 + to_m;
    if (from_min > to_min) {
        return current_min >= from_min || current_min <= to_min;
    }
    return current_min >= from_min && current_min <= to_min;
}

static bool night_mode_should_apply(void)
{
    // Check if night mode is enabled
    if (!s_night_mode_config.enable) {
        return false;
    }
    // Check if this period is suppressed
    if (s_night_mode_config.suppressed_this_period) {
        return false;
    }
    // Check if current time is in night period
    return night_mode_is_in_time_range(s_night_mode_config.from, s_night_mode_config.to);
}

static void night_mode_apply(void)
{
    if (screen_is_locked()) {
        return;
    }
    uint8_t target_br = s_night_mode_config.brightness;
    ESP_LOGI(TAG, "Night mode applied: brightness=%d", target_br);
    bsp_lcd_br_set(target_br);
}

static void night_mode_restore(void)
{
    if (screen_is_locked()) {
        return;
    }
    device_info_t *dev_info = webserver_config_get_device_info();
    uint8_t manual_br = dev_info ? dev_info->brightness : 50;
    ESP_LOGI(TAG, "Night mode exited, restoring manual brightness=%d", manual_br);
    bsp_lcd_br_set(manual_br);
}


static void night_mode_timer_cb(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Night mode timer callback");
    bool was_in_period = s_in_night_period;
    s_in_night_period = night_mode_is_in_time_range(
        s_night_mode_config.from, s_night_mode_config.to);
    if(s_in_night_period && !was_in_period) {
        s_night_mode_config.suppressed_this_period = false;
    }
    if(screen_is_locked()) {
        return;
    }
    bool should_apply = night_mode_should_apply();
    if(should_apply && (!was_in_period || s_night_mode_config.suppressed_this_period == false)) {
        night_mode_apply();
    }else if(!should_apply && was_in_period) {
        night_mode_restore();
    }
}

esp_err_t night_mode_save_config(const night_mode_config_t *config)
{
    if (!config) {
        return ESP_FAIL;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_FAIL;
    }
    cJSON_AddBoolToObject(root, "enable", config->enable);
    cJSON_AddNumberToObject(root, "brightness", config->brightness);
    cJSON_AddStringToObject(root, "from", config->from);
    cJSON_AddStringToObject(root, "to", config->to);
    cJSON_AddBoolToObject(root, "suppressed_this_period", config->suppressed_this_period);
    if(!little_file_system_write_json_to_file(root, NIGHT_MODE_CONFIG_FILE)) {
        return ESP_FAIL;
    }
    memcpy(&s_night_mode_config, config, sizeof(night_mode_config_t));
    return ESP_OK;
}

esp_err_t night_mode_load_config(night_mode_config_t *config)
{
    if (!config) {
        return ESP_FAIL;
    }
    cJSON *root = little_file_system_read_file_to_json(NIGHT_MODE_CONFIG_FILE);
    if (!root) {
        return ESP_FAIL;
    }
    cjson_parse_bool_field(root, "enable", &config->enable);
    cJSON *json_br = cJSON_GetObjectItem(root, "brightness");
    if(json_br && cJSON_IsNumber(json_br)) {
        int br =json_br->valueint;
        config->brightness = (br >= 1 && br <=50) ? (uint8_t)br : 20;
    }
    char *str;
    if(cjson_parse_string_field(root, "from", &str)) {
        strlcpy(config->from, str,NIGHT_MODE_TIME_STR_LEN);
    }
    if(cjson_parse_string_field(root, "to", &str)) {
        strlcpy(config->to, str,NIGHT_MODE_TIME_STR_LEN);
    }
    return ESP_OK;
}

bool night_mode_notify_manual_brightness(void)
{
    if (!s_night_mode_config.enable) {
        return false;
    }
    if(!s_in_night_period) {
        return false;
    }
    s_night_mode_config.suppressed_this_period = true;
    night_mode_save_config(&s_night_mode_config);
    ESP_LOGI(TAG, "Night mode suppressed for this period by manual brightness");
    return true;
}

uint8_t night_mode_get_effective_brightness(uint8_t manual_brightness)
{
    if(night_mode_should_apply()) {
        return s_night_mode_config.brightness;
    }
    return manual_brightness;
}

esp_err_t night_mode_api_set_config(bool enable, uint8_t brightness,
                                    const char *from, const char *to)
{
    night_mode_config_t config = {0};
    memcpy(&config, &s_night_mode_config, sizeof(night_mode_config_t));
    config.enable = enable;
    config.brightness = brightness;
    strlcpy(config.from, from,NIGHT_MODE_TIME_STR_LEN);
    strlcpy(config.to, to,NIGHT_MODE_TIME_STR_LEN);
    esp_err_t ret = night_mode_save_config(&config);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save night mode config: %s", esp_err_to_name(ret));
        return ret;
    }
    s_in_night_period = night_mode_is_in_time_range(config.from, config.to);
    s_night_mode_config.suppressed_this_period = false;
    if(screen_is_locked()) {
        return ESP_OK;
    }
    if (night_mode_should_apply()) {
        night_mode_apply();
    } else {
        night_mode_restore();
    }
    return ESP_OK;
}

void night_mode_init(void)
{
    if(night_mode_load_config(&s_night_mode_config) != ESP_OK) {
        s_night_mode_config.enable = false;
        s_night_mode_config.brightness = 20;
        strlcpy(s_night_mode_config.from, "20:00", NIGHT_MODE_TIME_STR_LEN);
        strlcpy(s_night_mode_config.to, "07:00", NIGHT_MODE_TIME_STR_LEN);
        night_mode_save_config(&s_night_mode_config);
        ESP_LOGI(TAG, "Night mode initialized with defaults");
    } else {
        ESP_LOGI(TAG, "Night mode config loaded: enable=%d, br=%d, %s-%s",
                 s_night_mode_config.enable, s_night_mode_config.brightness,
                 s_night_mode_config.from, s_night_mode_config.to);
    }
    s_in_night_period = night_mode_is_in_time_range(
        s_night_mode_config.from, s_night_mode_config.to);
    if(night_mode_should_apply()) {
        if (!screen_is_locked() && night_mode_should_apply())
        {
            night_mode_apply();
        }
    }
    s_night_mode_timer = xTimerCreate(
        "night_mode",
        pdMS_TO_TICKS(NIGHT_MODE_CHECK_PERIOD_MS),
        pdTRUE,
        (void *)0,
        night_mode_timer_cb
    );
    if (s_night_mode_timer) {
        xTimerStart(s_night_mode_timer, 0);
        ESP_LOGI(TAG, "Night mode periodic timer started (%d ms)", NIGHT_MODE_CHECK_PERIOD_MS);
    } else {
        ESP_LOGE(TAG, "Failed to create night mode timer");
    }
}
