#ifndef HA_LVGL_PSRAM_POOL_H
#define HA_LVGL_PSRAM_POOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *ha_lvgl_psram_pool_alloc(size_t size);

#ifdef __cplusplus
}
#endif

#endif
