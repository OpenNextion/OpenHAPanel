#ifndef OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_LIST_H
#define OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_LIST_H

#include <stddef.h>
#include <time.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HA_HISTORY_LIST_DEFAULT_WIDTH 280

typedef struct {
    time_t epoch;
    const char *text;
} ha_history_list_item_t;

typedef struct {
    /** Optional heading. Defaults to "History". */
    const char *title;
    /** Newest-first text records. Text is copied into LVGL labels. */
    const ha_history_list_item_t *items;
    size_t item_count;
    /** When non-empty, show this text instead of the records. */
    const char *message;
} ha_history_list_config_t;

/**
 * Create a textual history card grouped by local calendar date.
 *
 * Each record is rendered as left-aligned text and a right-aligned local
 * time. The caller retains ownership of config, items, and item text.
 */
lv_obj_t *ha_history_list_create(
    lv_obj_t *parent,
    const ha_history_list_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_LIST_H
