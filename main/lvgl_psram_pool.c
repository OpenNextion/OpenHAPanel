#include "ha_lvgl_psram_pool.h"

#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

/* LVGL calls this once while initializing its built-in TLSF allocator. */
void *ha_lvgl_psram_pool_alloc(size_t size)
{
    static void *pool;
    if (!pool) {
        pool = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!pool) {
            ESP_LOGE("lvgl_psram", "Unable to allocate %u-byte LVGL base pool in PSRAM",
                     (unsigned)size);
            abort();
        }
        ESP_LOGI("lvgl_psram", "LVGL base pool in PSRAM: %u bytes", (unsigned)size);
    }
    return pool;
}
