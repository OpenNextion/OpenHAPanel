#ifndef OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_CHART_H
#define OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_CHART_H

#include <stddef.h>
#include <time.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HA_HISTORY_CHART_DEFAULT_WIDTH 280
#define HA_HISTORY_CHART_DEFAULT_HEIGHT 304

typedef struct {
    time_t epoch;
    double value;
} ha_history_chart_point_t;

typedef struct {
    /** Optional heading. Defaults to "History". */
    const char *title;
    /** Unit appended directly to the selected value in the tooltip. */
    const char *unit;
    /** Chronological samples shown between start_time and end_time. */
    const ha_history_chart_point_t *points;
    size_t point_count;
    time_t start_time;
    time_t end_time;
    /** When non-empty, show this text instead of constructing the chart. */
    const char *message;
} ha_history_chart_config_t;

/**
 * Create a reusable HA history chart container.
 *
 * The component copies values needed by its tooltip, so the caller retains
 * ownership of config and points. Delete the returned object with
 * lv_obj_delete().
 */
lv_obj_t *ha_history_chart_create(
    lv_obj_t *parent,
    const ha_history_chart_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_CHART_H
