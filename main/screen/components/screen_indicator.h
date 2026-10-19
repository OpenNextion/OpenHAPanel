#ifndef OPEN_NEXTION_MAIN_SCREEN_SCREEN_INDICATOR_H
#define OPEN_NEXTION_MAIN_SCREEN_SCREEN_INDICATOR_H

#include <stddef.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct screen_indicator screen_indicator_t;

/**
 * Create a non-interactive page indicator at the bottom of parent.
 * Returns NULL when page_count is less than two or creation fails.
 */
screen_indicator_t *screen_indicator_create(lv_obj_t *parent,
                                            size_t page_count);

/** Release the component wrapper. Its LVGL object tree is owned by parent. */
void screen_indicator_destroy(screen_indicator_t *indicator);

/** Select the dot corresponding to the zero-based page position. */
void screen_indicator_set_active(screen_indicator_t *indicator,
                                 size_t active_index);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_SCREEN_INDICATOR_H
