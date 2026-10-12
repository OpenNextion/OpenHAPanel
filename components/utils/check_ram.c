#include "check_ram.h"
#include <stdint.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define CHECK_RAM_INTERNAL_MS 10000 /* Check RAM usage every 10s */

static const char *TAG = "check_ram";

static TimerHandle_t check_ram_timer_handle = NULL;

/**
 * @brief Checks internal RAM usage and triggers an auto-restart if critically low.
 */
static void check_internal_ram(TimerHandle_t xTimer)
{
    (void)xTimer;

    uint32_t total_free_heap = esp_get_free_heap_size();
    uint32_t total_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t free_psram_8bit = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t internal_largest_blk = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    uint32_t internal_8bit_free = total_free_8bit - free_psram_8bit;

    ESP_LOGI(TAG, "[RAM Check]: Total Free : %lu bytes, Internal Free : %lu bytes , Largest Internal Block : %lu bytes",
             total_free_heap, internal_8bit_free, internal_largest_blk);

    // if (webserver_logger_get_debug_flag()) {
    //     char *buf = malloc(1024);
    //     vTaskList(buf);
    //     printf("Name         State Prio Stack Num\n");
    //     printf("%s\n", buf);
    //     free(buf);
    // }
}

void check_ram_timer_init(void)
{
    /* Wi-Fi connect timer */
    check_ram_timer_handle = xTimerCreate("check_internal_ram", pdMS_TO_TICKS(CHECK_RAM_INTERNAL_MS), pdTRUE, 0, check_internal_ram);
    xTimerStart(check_ram_timer_handle, 100);
}
