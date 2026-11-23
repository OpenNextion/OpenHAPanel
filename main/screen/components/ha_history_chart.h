#ifndef OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_CHART_H
#define OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_CHART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HA_HISTORY_CHART_DEFAULT_WIDTH_PORTAIT 280
#define HA_HISTORY_CHART_DEFAULT_WIDTH_LANDSCAPE 440

#define HA_HISTORY_CHART_DEFAULT_HEIGHT_PORTAIT 302
#define HA_HISTORY_CHART_DEFAULT_HEIGHT_LANDSCAPE 164

typedef struct {
    time_t epoch;
    double value;
    /* Statistics buckets only: end of interval and the displayed band bounds. */
    time_t end_epoch;
    uint32_t run_id;
    double lower;
    double upper;
    bool has_band;
    bool has_line;
    /* Preserve the fields actually present in each statistics bucket for
     * tooltip rows; the plotted band bounds can contain fallback values. */
    double stat_state;
    double stat_min;
    double stat_mean;
    double stat_max;
    bool has_stat_state;
    bool has_stat_min;
    bool has_stat_mean;
    bool has_stat_max;
} ha_history_chart_point_t;

typedef struct {
    /** Optional heading. Defaults to "History". */
    const char *title;
    /** Unit appended directly to the selected value in the tooltip. */
    const char *unit;
    /** Chronological samples shown between start_time and end_time. */
    const ha_history_chart_point_t *points;
    size_t point_count;
    /** Statistics use a smooth mean/state line and optional min/max band. */
    bool statistics;
    time_t start_time;
    time_t end_time;
    /** When non-empty, show this text instead of constructing the chart. */
    const char *message;
} ha_history_chart_config_t;

/**
 * Create a reusable HA history chart container.
 *
 * The component copies the points used for drawing and tooltips, so the
 * caller retains ownership of config and points. Delete the returned object with
 * lv_obj_delete().
 */
lv_obj_t *ha_history_chart_create(
    lv_obj_t *parent,
    const ha_history_chart_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_HA_HISTORY_CHART_H
