#ifndef OPEN_NEXTION_MAIN_SCREEN_NOTIFICATION_H
#define OPEN_NEXTION_MAIN_SCREEN_NOTIFICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NOTIFICATION_LEVEL_INFO,
    NOTIFICATION_LEVEL_SUCCESS,
    NOTIFICATION_LEVEL_WARNING,
    NOTIFICATION_LEVEL_ERROR,
    NOTIFICATION_LEVEL_INACTIVE,
} notification_level_t;

typedef struct notification_t notification_t;

typedef void (*notification_dialog_click_cb_t)(notification_t *notification, void *user_data);
typedef void (*notification_swipe_down_cb_t)(notification_t *notification, void *user_data);

typedef enum {
    NOTIFICATION_CLOSE_PROGRAMMATIC,
    NOTIFICATION_CLOSE_USER,
    NOTIFICATION_CLOSE_TIMEOUT,
    NOTIFICATION_CLOSE_PARENT_DELETED,
} notification_close_reason_t;

typedef void (*notification_closed_cb_t)(notification_t *notification,
                                         notification_close_reason_t reason,
                                         void *user_data);

typedef struct {
    const char *title;
    /**
     * LVGL image sources. Each source can be an LV_SYMBOL_* string, an
     * lv_image_dsc_t pointer, or another source accepted by lv_image_set_src().
     * Image descriptors must remain valid while the notification is visible.
     */
    const void *icon_src;
    const void *status_icon_src;
    const char *status_text;
    const char *description;
    uint32_t durations;
    const char *tips;
    bool closeable;
    /** The title image already contains its own colored background. */
    bool icon_includes_background;
    /** Optional title icon size; zero uses the component default. */
    uint16_t icon_size;
    /** Place the status row below the title, alongside the title icon. */
    bool status_in_header;
    /** Keep the page below the notification at its original brightness. */
    bool transparent_overlay;
    /** Optional 0xRRGGBB overrides; zero keeps the component default. */
    uint32_t dialog_background_color;
    uint32_t accent_color;
    uint32_t description_color;
    notification_level_t level;
    notification_dialog_click_cb_t dialog_click_cb;
    void *dialog_click_user_data;
    /** Optional action invoked when the user swipes down over the notification. */
    notification_swipe_down_cb_t swipe_down_cb;
    void *swipe_down_user_data;
    notification_closed_cb_t closed_cb;
    void *closed_user_data;
} notification_config_t;

/**
 * Show a modal notification on LVGL's system layer.
 *
 * Multiple notification instances can be shown at the same time. The returned
 * handle can be passed to notification_close(). durations is expressed in
 * milliseconds; 0 disables the automatic close timer.
 */
notification_t *notification_show(const notification_config_t *config);

/** Close a displayed notification instance. */
void notification_close(notification_t *notification);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_NOTIFICATION_H
