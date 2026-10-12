#ifndef OPEN_NEXTION_MAIN_SCREEN_STATUS_BAR_H
#define OPEN_NEXTION_MAIN_SCREEN_STATUS_BAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

typedef struct status_bar status_bar_t;

typedef struct {
    const char *symbol;
    lv_color_t color;
    bool visible;
} status_bar_icon_config_t;

typedef struct {
    const char *title;
    const char *time_text;
    const status_bar_icon_config_t *icons;
    size_t icon_count;
} status_bar_config_t;

status_bar_t *status_bar_create(lv_obj_t *parent, const status_bar_config_t *config);
void status_bar_destroy(status_bar_t *bar);

lv_obj_t *status_bar_get_obj(status_bar_t *bar);

void status_bar_set_title(status_bar_t *bar, const char *title);
void status_bar_set_time(status_bar_t *bar, const char *time_text);
void status_bar_set_icons(status_bar_t *bar, const status_bar_icon_config_t *icons, size_t icon_count);
void status_bar_set_status_list(status_bar_t *bar, const status_bar_icon_config_t *icons, size_t icon_count);
void status_bar_update_icon(status_bar_t *bar, size_t index, const status_bar_icon_config_t *icon);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_STATUS_BAR_H
