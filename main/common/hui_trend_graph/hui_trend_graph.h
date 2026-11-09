#ifndef OPEN_NEXTION_HUI_TREND_GRAPH_H
#define OPEN_NEXTION_HUI_TREND_GRAPH_H

#include <stddef.h>
#include <time.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HUI_TREND_GRAPH_MAX_POINTS 96

typedef struct {
    time_t epoch;
    double value;
} hui_trend_graph_point_t;

/* A transparent, noninteractive 56px graph for a Tile Feature host. */
lv_obj_t *hui_trend_graph_create(lv_obj_t *parent);

/* Points must be chronological and fit inside the supplied time window. */
void hui_trend_graph_set_points(lv_obj_t *graph,
                                const hui_trend_graph_point_t *points,
                                size_t count,
                                time_t start_time,
                                time_t end_time,
                                bool animate);

void hui_trend_graph_set_color(lv_obj_t *graph, lv_color_t color);

#ifdef __cplusplus
}
#endif

#endif
