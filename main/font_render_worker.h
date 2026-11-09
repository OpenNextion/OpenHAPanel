#ifndef HA_FONT_RENDER_WORKER_H
#define HA_FONT_RENDER_WORKER_H

#include <stdbool.h>
#include "lvgl.h"

/* Move FreeType cache misses off the deeply nested LVGL drawing stack. */
bool ha_font_render_worker_attach(lv_font_t *font);

#endif
