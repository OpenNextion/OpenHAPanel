#ifndef OPEN_NEXTION_MAIN_SCREEN_COMPONENTS_TOAST_H
#define OPEN_NEXTION_MAIN_SCREEN_COMPONENTS_TOAST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct toast toast_t;

typedef void (*toast_closed_cb_t)(toast_t *toast, void *user_data);

typedef struct {
    const char *title;
    /** Image source accepted by lv_image_set_src(). */
    const void *icon_src;
    /** Zero keeps the Toast visible until toast_close() is called. */
    uint32_t duration_ms;
    uint32_t background_color;
    /** LVGL background opacity, for example LV_OPA_80. */
    lv_opa_t background_opa;
    int16_t width;
    int16_t height;
    int16_t x;
    int16_t y;
    int16_t radius;
    /** Draw below the parent's existing children, for example below a status bar. */
    bool behind_siblings;
    toast_closed_cb_t closed_cb;
    void *closed_user_data;
} toast_config_t;

/** Create a non-modal Toast as a child of parent. */
toast_t *toast_show(lv_obj_t *parent, const toast_config_t *config);

/** Close a Toast. Calling this repeatedly is safe. */
void toast_close(toast_t *toast);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_COMPONENTS_TOAST_H
