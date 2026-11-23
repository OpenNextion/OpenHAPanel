#include "ui_i18n.h"
#include "ha_ui_font.h"
#include "i18n.h"
#include "ha_history_chart.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "assets/ha_ui_assets.h"
#include "entity_handlers/light/components/light_detail_theme.h"
#include "sys_time_api.h"
#include "bsp_lcd.h"

#define HA_HISTORY_TIME_LABEL_COUNT 5
/* LVGL's lv_map() multiplies int32_t values; keep its Y coordinates small. */
#define HA_HISTORY_DRAW_Y_MAX 10000
#define HA_HISTORY_CHART_Y_PORT 57
#define HA_HISTORY_CHART_Y_LAND 37
#define HA_HISTORY_CHART_RIGHT 12
#define HA_HISTORY_PLOT_HEIGHT_PORT 217
#define HA_HISTORY_PLOT_HEIGHT_LAND 103

static inline int32_t history_chart_y(void)
{
    return bsp_lcd_is_landscape() ? HA_HISTORY_CHART_Y_LAND
                                  : HA_HISTORY_CHART_Y_PORT;
}
static inline int32_t history_plot_height(void)
{
    return bsp_lcd_is_landscape() ? HA_HISTORY_PLOT_HEIGHT_LAND
                                  : HA_HISTORY_PLOT_HEIGHT_PORT;
}
#define HA_HISTORY_PLOT_PAD_LEFT 0
#define HA_HISTORY_PLOT_PAD_RIGHT 12
#define HA_HISTORY_PLOT_PAD_VER 12
#define HA_HISTORY_UNIT_MAX 32
#define HA_HISTORY_Y_AXIS_MAX_TICKS 5
#define HA_HISTORY_Y_LABEL_X 4
#define HA_HISTORY_Y_LABEL_GAP 6
#define HA_HISTORY_TIME_LABEL_GAP 4
#define HA_HISTORY_STEP_LINE_POINTS 64

typedef struct {
    lv_obj_t *chart;
    lv_obj_t *statistics_canvas;
    lv_obj_t *statistics_cursor;
    lv_obj_t *tooltip;
    lv_obj_t *tooltip_time;
    lv_obj_t *tooltip_values;
    lv_indev_t *indev;
    lv_chart_series_t *series;
    lv_chart_cursor_t *cursor;
    ha_history_chart_point_t *points;
    void *statistics_pixels;
    lv_point_precise_t statistics_cursor_points[2];
    size_t point_count;
    bool statistics;
    bool series_has_stat_state;
    bool series_has_stat_min;
    bool series_has_stat_mean;
    bool series_has_stat_max;
    double axis_min;
    double axis_max;
    time_t start_time;
    time_t end_time;
    int32_t chart_x;
    int32_t chart_width;
    char unit[HA_HISTORY_UNIT_MAX];
} ha_history_chart_t;

static void history_chart_event_cb(lv_event_t *event);
static void history_chart_indev_pressed_cb(lv_event_t *event);
static void history_chart_delete_cb(lv_event_t *event);

static int32_t history_chart_draw_y(double value,
                                    double axis_min,
                                    double axis_max)
{
    const double fraction = LV_CLAMP(0.0,
        (value - axis_min) / (axis_max - axis_min), 1.0);
    return (int32_t)lround(fraction * HA_HISTORY_DRAW_Y_MAX);
}

static void history_chart_format_value(double value,
                                       char *output,
                                       size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }

    const i18n_number_options_t options = {
        true, I18N_NUMBER_FRACTION_MAXIMUM, 2
    };
    if (i18n_format_number_double(output, output_size, value, &options)) {
        return;
    }

    /* A bounded locale-independent fallback keeps the chart usable if an
     * extreme value cannot fit in the localized output buffer. */
    snprintf(output, output_size, "%.2f", value);
    size_t length = strlen(output);
    while (length > 0 && output[length - 1] == '0') {
        output[--length] = '\0';
    }
    if (length > 0 && output[length - 1] == '.') {
        output[--length] = '\0';
    }
    if (strcmp(output, "-0") == 0) {
        snprintf(output, output_size, "0");
    }
}

static void history_chart_format_tooltip_time(time_t epoch,
                                              char *output,
                                              size_t output_size)
{
    char local_iso[40] = {0};
    if (epoch >= 0 &&
        sys_time_format_iso8601_time_by_utc_ms(
            (uint64_t)epoch * 1000, local_iso, sizeof(local_iso)) == 0 &&
        strlen(local_iso) >= 19) {
        unsigned year, month, day, hour, minute, second;
        if (sscanf(local_iso, "%4u-%2u-%2uT%2u:%2u:%2u",
                   &year, &month, &day, &hour, &minute, &second) == 6) {
            char date[96];
            char time_text[24];
            if (i18n_format_date(date, sizeof(date), year, month, day,
                                 false)) {
                snprintf(time_text, sizeof(time_text), "%02u:%02u:%02u",
                             hour, minute, second);
                const i18n_arg_t args[] = {
                    {"date", date}, {"time", time_text}
                };
                if (i18n_format(output, output_size,
                                i18n_get(I18N_DATE_TIME), args, 2)) {
                    return;
                }
            }
        }
    }
    snprintf(output, output_size, "--");
}

static void history_chart_append_tooltip_value(char *output,
                                               size_t output_size,
                                               const char *name,
                                               double value,
                                               const char *unit)
{
    const size_t used = strlen(output);
    if (used >= output_size) {
        return;
    }
    char formatted[32];
    history_chart_format_value(value, formatted, sizeof(formatted));
    snprintf(output + used, output_size - used, "%s%s%s%s",
             used ? "\n" : "", name ? name : "",
             formatted, unit);
}

static double history_chart_nice_step(double minimum_step)
{
    if (!isfinite(minimum_step) || minimum_step <= 0.0) {
        return 1.0;
    }

    const double exponent = floor(log10(minimum_step));
    const double magnitude = pow(10.0, exponent);
    const double fraction = minimum_step / magnitude;
    double nice_fraction = 10.0;
    if (fraction <= 1.0) {
        nice_fraction = 1.0;
    } else if (fraction <= 2.0) {
        nice_fraction = 2.0;
    } else if (fraction <= 5.0) {
        nice_fraction = 5.0;
    }
    return nice_fraction * magnitude;
}

static void history_chart_calculate_y_axis(
    const ha_history_chart_point_t *points,
    size_t point_count,
    bool statistics,
    double *axis_min,
    double *axis_max,
    size_t *tick_count)
{
    double min_value = points[0].value;
    double max_value = min_value;
    for (size_t i = 0; i < point_count; i++) {
        const double value = points[i].value;
        min_value = LV_MIN(min_value, value);
        max_value = LV_MAX(max_value, value);
        if (statistics && points[i].has_band) {
            min_value = LV_MIN(min_value, points[i].lower);
            max_value = LV_MAX(max_value, points[i].upper);
        }
    }

    const double minimum = min_value;
    const double maximum = max_value;
    double step;
    if (maximum > minimum) {
        step = history_chart_nice_step((maximum - minimum) /
                                       (HA_HISTORY_Y_AXIS_MAX_TICKS - 1));
    } else {
        /* A flat series still needs enough vertical range to be readable. */
        step = history_chart_nice_step(LV_MAX(fabs(minimum) * 0.1, 0.01));
    }

    double low = floor(minimum / step) * step;
    double high = ceil(maximum / step) * step;
    if (maximum == minimum) {
        low -= 2.0 * step;
        high += 2.0 * step;
    }

    size_t count = (size_t)llround((high - low) / step) + 1;
    while (count > HA_HISTORY_Y_AXIS_MAX_TICKS) {
        step = history_chart_nice_step(step * 1.01);
        low = floor(minimum / step) * step;
        high = ceil(maximum / step) * step;
        count = (size_t)llround((high - low) / step) + 1;
    }

    *axis_min = low;
    *axis_max = high;
    *tick_count = count;
}

static int32_t history_chart_create_y_axis_labels(lv_obj_t *root,
                                                  double axis_min,
                                                  double axis_max,
                                                  size_t tick_count)
{
    if (!root || tick_count < 2 || axis_max <= axis_min) {
        return HA_HISTORY_Y_LABEL_X + HA_HISTORY_Y_LABEL_GAP;
    }

    lv_obj_t *labels[HA_HISTORY_Y_AXIS_MAX_TICKS] = {0};
    int32_t label_width = 0;
    const int32_t plot_top = history_chart_y() + HA_HISTORY_PLOT_PAD_VER;
    const int32_t plot_height = history_plot_height() -
                                HA_HISTORY_PLOT_PAD_VER * 2;
    const double range = (double)axis_max - axis_min;
    for (size_t i = 0; i < tick_count; i++) {
        const double scaled_value = axis_max -
            range * i / (tick_count - 1);
        char value_text[32] = {0};
        history_chart_format_value(scaled_value,
                                   value_text,
                                   sizeof(value_text));

        lv_obj_t *label = lv_label_create(root);
        ui_i18n_label_set_text(label, value_text);
        lv_obj_set_style_text_font(label, ha_ui_font_regular(12), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
        lv_obj_update_layout(label);
        labels[i] = label;
        label_width = LV_MAX(label_width, lv_obj_get_width(label));
        const int32_t center_y = plot_top + (int32_t)(plot_height * i /
                                                       (tick_count - 1));
        lv_obj_set_y(label, center_y - lv_obj_get_height(label) / 2);
    }

    for (size_t i = 0; i < tick_count; i++) {
        lv_obj_set_width(labels[i], label_width);
        lv_obj_set_style_text_align(labels[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_x(labels[i], HA_HISTORY_Y_LABEL_X);
    }
    return HA_HISTORY_Y_LABEL_X + label_width + HA_HISTORY_Y_LABEL_GAP;
}

static void history_chart_render_message(lv_obj_t *root, const char *text)
{
    lv_obj_t *label = lv_label_create(root);
    /* The root has not been laid out yet, so its measured width is stale. */
    const int32_t chart_width = bsp_lcd_is_landscape()
                                    ? HA_HISTORY_CHART_DEFAULT_WIDTH_LANDSCAPE
                                    : HA_HISTORY_CHART_DEFAULT_WIDTH_PORTAIT;
    lv_obj_set_width(label, chart_width - 24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    ui_i18n_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, ha_ui_font_regular(16), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(label, 12, 72);
}

static void history_chart_create_time_labels(
    lv_obj_t *root,
    time_t start_time,
    time_t end_time,
    int32_t chart_x,
    int32_t chart_width)
{
    const time_t span = end_time > start_time ? end_time - start_time : 1;
    const int32_t plot_start = chart_x + HA_HISTORY_PLOT_PAD_LEFT;
    const int32_t plot_width = chart_width -
                               HA_HISTORY_PLOT_PAD_LEFT -
                               HA_HISTORY_PLOT_PAD_RIGHT;

    for (size_t i = 0; i < HA_HISTORY_TIME_LABEL_COUNT; i++) {
        const time_t epoch = start_time +
                             (time_t)((int64_t)span * i /
                                      (HA_HISTORY_TIME_LABEL_COUNT - 1));
        char time_text[9] = "--:--";
        char local_iso[40] = {0};
        if (sys_time_format_iso8601_time_by_utc_ms(
                (uint64_t)epoch * 1000,
                local_iso,
                sizeof(local_iso)) == 0 &&
            strlen(local_iso) >= 19) {
            if (span > 2 * 24 * 60 * 60 ||
                strncmp(local_iso + 11, "00:00:00", 8) == 0) {
                snprintf(time_text, sizeof(time_text), "%.2s/%.2s",
                         local_iso + 5, local_iso + 8);
            } else if (span < 5 * 60) {
                snprintf(time_text, sizeof(time_text), "%.8s", local_iso + 11);
            } else {
                snprintf(time_text, sizeof(time_text), "%.5s", local_iso + 11);
            }
        }

        lv_obj_t *label = lv_label_create(root);
        ui_i18n_label_set_text(label, time_text);
        lv_obj_set_style_text_font(label, ha_ui_font_regular(12), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
        lv_obj_update_layout(label);
        const int32_t center_x = plot_start +
                                 (int32_t)(plot_width * i /
                                           (HA_HISTORY_TIME_LABEL_COUNT - 1));
        lv_obj_set_pos(label,
                       center_x - lv_obj_get_width(label) / 2,
                       history_chart_y() + history_plot_height() +
                           HA_HISTORY_TIME_LABEL_GAP);
    }
}

static void history_chart_draw_step_line(lv_event_t *event,
                                         ha_history_chart_t *history)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    if (!layer || !history || !history->chart || !history->series ||
        history->point_count == 0) {
        return;
    }

    lv_area_t chart_coords;
    lv_obj_get_coords(history->chart, &chart_coords);
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x4269d0);
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;

    lv_point_precise_t line_points[HA_HISTORY_STEP_LINE_POINTS];
    lv_point_t point;
    lv_chart_get_point_pos_by_id(history->chart, history->series, 0, &point);
    line_points[0] = (lv_point_precise_t){
        .x = chart_coords.x1 + point.x,
        .y = chart_coords.y1 + point.y,
    };
    int32_t line_count = 1;

    for (size_t i = 1; i < history->point_count; i++) {
        lv_chart_get_point_pos_by_id(history->chart,
                                     history->series,
                                     (uint32_t)i,
                                     &point);
        if (line_count + 2 > HA_HISTORY_STEP_LINE_POINTS) {
            line_dsc.points = line_points;
            line_dsc.point_cnt = line_count;
            lv_draw_line(layer, &line_dsc);
            line_points[0] = line_points[line_count - 1];
            line_count = 1;
        }
        /* Hold the previous value until this sample, then change vertically. */
        const int32_t x = chart_coords.x1 + point.x;
        line_points[line_count] = (lv_point_precise_t){
            .x = x,
            .y = line_points[line_count - 1].y,
        };
        line_count++;
        line_points[line_count++] = (lv_point_precise_t){
            .x = x,
            .y = chart_coords.y1 + point.y,
        };
    }

    /* Keep the latest state visible through the end of the time window. */
    const int32_t plot_right = chart_coords.x2 - HA_HISTORY_PLOT_PAD_RIGHT;
    line_points[line_count] = (lv_point_precise_t){
        .x = LV_MAX(plot_right, (int32_t)line_points[line_count - 1].x),
        .y = line_points[line_count - 1].y,
    };
    line_count++;
    line_dsc.points = line_points;
    line_dsc.point_cnt = line_count;
    lv_draw_line(layer, &line_dsc);
}

static double history_chart_band_center(const ha_history_chart_point_t *point)
{
    return point->has_band ? (point->lower + point->upper) / 2.0
                           : point->value;
}

/* Keep a single index for mean and both band edges. The frontend selects
 * ECharts LTTB for stacked bands for the same alignment reason. */
static size_t history_chart_lttb_run(ha_history_chart_point_t *points,
                                      size_t start, size_t count,
                                      size_t target, size_t output)
{
    if (count <= target || count < 3) {
        memmove(points + output, points + start,
                count * sizeof(*points));
        return output + count;
    }
    const double bucket_width = (double)(count - 2) / (target - 2);
    size_t previous = start;
    points[output++] = points[start];
    for (size_t bucket = 0; bucket < target - 2; bucket++) {
        size_t next_begin = start +
            (size_t)floor((bucket + 1) * bucket_width) + 1;
        size_t next_end = start +
            (size_t)floor((bucket + 2) * bucket_width) + 1;
        if (next_end > start + count) next_end = start + count;
        double avg_x = 0.0;
        double avg_y = 0.0;
        for (size_t i = next_begin; i < next_end; i++) {
            avg_x += points[i].epoch;
            avg_y += history_chart_band_center(&points[i]);
        }
        const size_t avg_count = next_end - next_begin;
        if (avg_count) {
            avg_x /= avg_count;
            avg_y /= avg_count;
        } else {
            avg_x = points[start + count - 1].epoch;
            avg_y = history_chart_band_center(&points[start + count - 1]);
        }
        const size_t begin = start +
            (size_t)floor(bucket * bucket_width) + 1;
        size_t finish = start +
            (size_t)floor((bucket + 1) * bucket_width) + 1;
        if (finish >= start + count) finish = start + count - 1;
        size_t best = begin;
        double best_area = -1.0;
        for (size_t i = begin; i < finish; i++) {
            const double previous_y = history_chart_band_center(&points[previous]);
            const double area = fabs(
                ((double)points[previous].epoch - avg_x) *
                    (history_chart_band_center(&points[i]) - previous_y) -
                ((double)points[previous].epoch - points[i].epoch) *
                    (avg_y - previous_y));
            if (area > best_area) {
                best_area = area;
                best = i;
            }
        }
        const ha_history_chart_point_t chosen = points[best];
        points[output++] = chosen;
        previous = best;
    }
    points[output++] = points[start + count - 1];
    return output;
}

static size_t history_chart_lttb_reduce(ha_history_chart_point_t *points,
                                        size_t count, size_t target)
{
    if (count <= target || target < 3) {
        return count;
    }
    size_t output = 0;
    for (size_t start = 0; start < count;) {
        size_t end = start + 1;
        while (end < count && points[end].run_id == points[start].run_id) {
            end++;
        }
        const size_t run_count = end - start;
        size_t budget = (target * run_count) / count;
        budget = LV_MAX(run_count < 3 ? run_count : 3, budget);
        output = history_chart_lttb_run(points, start, run_count,
                                        budget, output);
        start = end;
    }
    return output;
}

static int32_t history_chart_stat_x(const ha_history_chart_t *history,
                                    const lv_area_t *coords, time_t epoch)
{
    const double span = (double)history->end_time - history->start_time;
    const double fraction = LV_CLAMP(0.0,
        ((double)epoch - history->start_time) / span, 1.0);
    const int32_t width = lv_obj_get_content_width(history->chart);
    return coords->x1 + HA_HISTORY_PLOT_PAD_LEFT +
           (int32_t)lround(fraction * width);
}

static int32_t history_chart_stat_y(const ha_history_chart_t *history,
                                    const lv_area_t *coords, double value)
{
    const double fraction = LV_CLAMP(0.0,
        (value - history->axis_min) /
            (history->axis_max - history->axis_min), 1.0);
    const int32_t height = lv_obj_get_content_height(history->chart);
    return coords->y1 + HA_HISTORY_PLOT_PAD_VER +
           (int32_t)lround((1.0 - fraction) * height);
}

static double history_chart_smooth_y(double y0, double y1,
                                      double y2, double y3,
                                      int32_t x0, int32_t x1,
                                      int32_t x2, int32_t x3,
                                      double t)
{
    const double dx = x2 - x1;
    if (dx == 0.0) return y2;
    const double first_slope = x2 != x0 ? (y2 - y0) / (x2 - x0)
                                        : (y2 - y1) / dx;
    const double last_slope = x3 != x1 ? (y3 - y1) / (x3 - x1)
                                       : (y2 - y1) / dx;
    const double low = fmin(y1, y2);
    const double high = fmax(y1, y2);
    const double control = dx * 0.4;
    const double c1 = LV_CLAMP(low, y1 + first_slope * control, high);
    const double c2 = LV_CLAMP(low, y2 - last_slope * control, high);
    const double reverse = 1.0 - t;
    return reverse * reverse * reverse * y1 +
           3.0 * reverse * reverse * t * c1 +
           3.0 * reverse * t * t * c2 + t * t * t * y2;
}

static void history_chart_draw_stat_segment(
    lv_layer_t *layer, const ha_history_chart_t *history,
    const lv_area_t *coords, const ha_history_chart_point_t *previous,
    const ha_history_chart_point_t *a,
    const ha_history_chart_point_t *b,
    const ha_history_chart_point_t *next, bool band)
{
    if (band) {
        if (!previous->has_band) previous = a;
        if (!next->has_band) next = b;
    }
    const int32_t x0 = history_chart_stat_x(history, coords, previous->epoch);
    const int32_t x1 = history_chart_stat_x(history, coords, a->epoch);
    const int32_t x2 = history_chart_stat_x(history, coords, b->epoch);
    const int32_t x3 = history_chart_stat_x(history, coords, next->epoch);
    if (x2 < x1 || (band && (!a->has_band || !b->has_band)) ||
        (!band && (!a->has_line || !b->has_line))) {
        return;
    }
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x4269d0);
    dsc.width = band ? 1 : 2;
    dsc.opa = band ? (lv_opa_t)64 : LV_OPA_COVER;
    int32_t old_y = history_chart_stat_y(history, coords, a->value);
    for (int32_t x = x1; x <= x2; x++) {
        const double t = x2 == x1 ? 1.0 :
                         (double)(x - x1) / (x2 - x1);
        if (band) {
            const int32_t upper = (int32_t)lround(history_chart_smooth_y(
                history_chart_stat_y(history, coords, previous->upper),
                history_chart_stat_y(history, coords, a->upper),
                history_chart_stat_y(history, coords, b->upper),
                history_chart_stat_y(history, coords, next->upper),
                x0, x1, x2, x3, t));
            const int32_t lower = (int32_t)lround(history_chart_smooth_y(
                history_chart_stat_y(history, coords, previous->lower),
                history_chart_stat_y(history, coords, a->lower),
                history_chart_stat_y(history, coords, b->lower),
                history_chart_stat_y(history, coords, next->lower),
                x0, x1, x2, x3, t));
            dsc.p1 = (lv_point_precise_t){ .x = x, .y = upper };
            dsc.p2 = (lv_point_precise_t){ .x = x, .y = lower };
        } else {
            const int32_t y = (int32_t)lround(history_chart_smooth_y(
                history_chart_stat_y(history, coords, previous->value),
                history_chart_stat_y(history, coords, a->value),
                history_chart_stat_y(history, coords, b->value),
                history_chart_stat_y(history, coords, next->value),
                x0, x1, x2, x3, t));
            dsc.p1 = (lv_point_precise_t){ .x = x == x1 ? x : x - 1,
                                            .y = old_y };
            dsc.p2 = (lv_point_precise_t){ .x = x, .y = y };
            old_y = y;
        }
        lv_draw_line(layer, &dsc);
    }
}

static void history_chart_draw_statistics(lv_layer_t *layer,
                                          ha_history_chart_t *history,
                                          const lv_area_t *coords)
{
    if (!layer || !history || !history->chart || !history->points || !coords) {
        return;
    }
    /* Draw the translucent range first, then the opaque mean/state line. */
    for (int pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < history->point_count; i++) {
            const ha_history_chart_point_t *a = &history->points[i];
            ha_history_chart_point_t held = *a;
            const bool contiguous = i + 1 < history->point_count &&
                                    a->run_id == history->points[i + 1].run_id;
            const ha_history_chart_point_t *b;
            if (contiguous) {
                b = &history->points[i + 1];
            } else if (a->end_epoch > a->epoch) {
                held.epoch = a->end_epoch;
                b = &held;
            } else {
                continue;
            }
            const ha_history_chart_point_t *previous =
                i > 0 && history->points[i - 1].run_id == a->run_id
                    ? &history->points[i - 1] : a;
            const ha_history_chart_point_t *next =
                contiguous && i + 2 < history->point_count &&
                b->run_id == history->points[i + 2].run_id
                    ? &history->points[i + 2] : b;
            history_chart_draw_stat_segment(layer, history, coords,
                                           previous, a, b, next,
                                           pass == 0);
        }
    }
}

static void history_chart_cache_statistics(lv_obj_t *root,
                                           ha_history_chart_t *history)
{
    const int32_t width = history->chart_width;
    const int32_t height = history_plot_height();
    if (width <= 0 || height <= 0) {
        return;
    }
    const uint32_t stride = lv_draw_buf_width_to_stride(
        width, LV_COLOR_FORMAT_ARGB8888);
    if (stride == 0 || (size_t)stride > SIZE_MAX / (size_t)height) {
        return;
    }
    void *pixels = heap_caps_malloc((size_t)stride * (size_t)height,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pixels) {
        return;
    }
    lv_obj_t *canvas = lv_canvas_create(root);
    if (!canvas) {
        heap_caps_free(pixels);
        return;
    }
    history->statistics_pixels = pixels;
    history->statistics_canvas = canvas;
    lv_obj_remove_style_all(canvas);
    lv_canvas_set_buffer(canvas, pixels, width, height,
                         LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(canvas, lv_color_hex(0x4269d0), LV_OPA_TRANSP);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(canvas, history->chart_x, history_chart_y());
    lv_obj_update_layout(history->chart);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    const lv_area_t coords = { .x1 = 0, .y1 = 0,
                               .x2 = width - 1, .y2 = height - 1 };
    history_chart_draw_statistics(&layer, history, &coords);
    lv_canvas_finish_layer(canvas, &layer);
}

static void history_chart_event_cb(lv_event_t *event)
{
    ha_history_chart_t *history = lv_event_get_user_data(event);
    if (!history) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DRAW_POST_END) {
        if (history->statistics && !history->statistics_canvas) {
            lv_area_t coords;
            lv_obj_get_coords(history->chart, &coords);
            history_chart_draw_statistics(lv_event_get_layer(event), history,
                                          &coords);
        } else if (!history->statistics) {
            history_chart_draw_step_line(event, history);
        }
        return;
    }
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    const uint32_t point_id = lv_chart_get_pressed_point(history->chart);
    if (point_id == LV_CHART_POINT_NONE ||
        point_id >= history->point_count) {
        return;
    }

    lv_point_t point;
    if (history->statistics) {
        lv_area_t coords;
        lv_obj_get_coords(history->chart, &coords);
        point.x = history_chart_stat_x(
            history, &coords, history->points[point_id].epoch) - coords.x1;
        point.y = history_chart_stat_y(
            history, &coords, history->points[point_id].value) - coords.y1;
        if (history->statistics_cursor) {
            lv_obj_set_pos(history->statistics_cursor,
                           history->chart_x + point.x, history_chart_y());
            lv_obj_remove_flag(history->statistics_cursor, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_chart_set_cursor_pos(history->chart, history->cursor, &point);
        }
    } else {
        lv_chart_get_point_pos_by_id(history->chart, history->series,
                                     point_id, &point);
        lv_chart_set_cursor_point(history->chart, history->cursor,
                                  history->series, point_id);
    }

    const ha_history_chart_point_t *selected = &history->points[point_id];
    char time_text[128];
    history_chart_format_tooltip_time(selected->epoch, time_text,
                                      sizeof(time_text));
    char values_text[320] = {0};
    if (history->statistics) {
        /* Match statistics-chart-data.ts: min is sorted before the other
         * series when a band exists. The band-top series always carries a
         * numeric value, using zero for a missing bucket field. */
        const bool band = (history->series_has_stat_min +
                           history->series_has_stat_mean +
                           history->series_has_stat_max) > 1;
        if (band && history->series_has_stat_min &&
            selected->has_stat_min) {
            history_chart_append_tooltip_value(values_text,
                sizeof(values_text), "min: ", selected->stat_min,
                history->unit);
        }
        if (history->series_has_stat_state && selected->has_stat_state) {
            history_chart_append_tooltip_value(values_text,
                sizeof(values_text), "state: ", selected->stat_state,
                history->unit);
        }
        if (!band && history->series_has_stat_min &&
            selected->has_stat_min) {
            history_chart_append_tooltip_value(values_text,
                sizeof(values_text), "min: ", selected->stat_min,
                history->unit);
        }
        if (history->series_has_stat_mean &&
            (selected->has_stat_mean ||
             (band && !history->series_has_stat_max))) {
            history_chart_append_tooltip_value(values_text,
                sizeof(values_text), "mean: ",
                selected->has_stat_mean ? selected->stat_mean : 0.0,
                history->unit);
        }
        if (history->series_has_stat_max &&
            (selected->has_stat_max || band)) {
            history_chart_append_tooltip_value(values_text,
                sizeof(values_text), "max: ",
                selected->has_stat_max ? selected->stat_max : 0.0,
                history->unit);
        }
    } else {
        history_chart_append_tooltip_value(values_text,
            sizeof(values_text), NULL, selected->value, history->unit);
    }
    if (!values_text[0]) {
        return;
    }
    lv_label_set_text(history->tooltip_time, time_text);
    lv_label_set_text(history->tooltip_values, values_text);
    lv_obj_remove_flag(history->tooltip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(history->tooltip);

    const int32_t tooltip_width = lv_obj_get_width(history->tooltip);
    const int32_t tooltip_height = lv_obj_get_height(history->tooltip);
    int32_t tooltip_x = history->chart_x + point.x + 8;
    int32_t chart_default_width = bsp_lcd_is_landscape() ? HA_HISTORY_CHART_DEFAULT_WIDTH_LANDSCAPE : HA_HISTORY_CHART_DEFAULT_WIDTH_PORTAIT;
    if (tooltip_x + tooltip_width > chart_default_width - 6) {
        tooltip_x = history->chart_x + point.x - tooltip_width - 8;
    }
    tooltip_x = LV_CLAMP(4,
                         tooltip_x,
                         chart_default_width - tooltip_width - 4);

    int32_t tooltip_y = history_chart_y() + point.y -
                        tooltip_height / 2;
    const int32_t root_height = bsp_lcd_is_landscape()
        ? HA_HISTORY_CHART_DEFAULT_HEIGHT_LANDSCAPE
        : HA_HISTORY_CHART_DEFAULT_HEIGHT_PORTAIT;
    const int32_t latest_y = LV_MAX(history_chart_y(),
                                   root_height - tooltip_height - 4);
    tooltip_y = LV_CLAMP(history_chart_y(), tooltip_y, latest_y);
    lv_obj_set_pos(history->tooltip, tooltip_x, tooltip_y);
}

static void history_chart_indev_pressed_cb(lv_event_t *event)
{
    ha_history_chart_t *history = lv_event_get_user_data(event);
    if (!history || !history->indev || !history->chart ||
        !history->tooltip ||
        lv_obj_has_flag(history->tooltip, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(history->indev, &point);
    lv_area_t chart_coords;
    lv_obj_get_coords(history->chart, &chart_coords);
    if (point.x >= chart_coords.x1 && point.x <= chart_coords.x2 &&
        point.y >= chart_coords.y1 && point.y <= chart_coords.y2) {
        return;
    }

    lv_obj_add_flag(history->tooltip, LV_OBJ_FLAG_HIDDEN);
    if (history->statistics_cursor) {
        lv_obj_add_flag(history->statistics_cursor, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_chart_set_cursor_point(history->chart,
                                  history->cursor,
                                  history->series,
                                  LV_CHART_POINT_NONE);
    }
}

static void history_chart_delete_cb(lv_event_t *event)
{
    ha_history_chart_t *history = lv_event_get_user_data(event);
    if (!history) {
        return;
    }
    if (history->indev) {
        lv_indev_remove_event_cb_with_user_data(
            history->indev,
            history_chart_indev_pressed_cb,
            history);
    }
    if (history->statistics_canvas) {
        lv_obj_delete(history->statistics_canvas);
    }
    if (history->statistics_cursor) {
        lv_obj_delete(history->statistics_cursor);
    }
    heap_caps_free(history->statistics_pixels);
    lv_free(history->points);
    lv_free(history);
}

static lv_obj_t *history_chart_create_root(lv_obj_t *parent,
                                           const char *title)
{
    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        return NULL;
    }
    lv_obj_remove_style_all(root);
    int32_t chart_default_width = bsp_lcd_is_landscape() ? HA_HISTORY_CHART_DEFAULT_WIDTH_LANDSCAPE : HA_HISTORY_CHART_DEFAULT_WIDTH_PORTAIT;
    int32_t chart_default_height = bsp_lcd_is_landscape() ? HA_HISTORY_CHART_DEFAULT_HEIGHT_LANDSCAPE : HA_HISTORY_CHART_DEFAULT_HEIGHT_PORTAIT;
    lv_obj_set_size(root,
                    chart_default_width,
                    chart_default_height);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(light_detail_theme_color(0x1E2228, LIGHT_DETAIL_LIGHT_SURFACE)), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root, 12, 0);

    lv_obj_t *title_label = lv_label_create(root);
    lv_obj_set_width(title_label, chart_default_width - 24);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(title_label, ha_ui_font_medium(20), 0);
    ui_i18n_label_set_text(title_label, title ? title : i18n_get(I18N_HISTORY_TITLE));
    lv_obj_set_style_text_color(title_label, lv_color_hex(light_detail_theme_color(0xFFFFFF, LIGHT_DETAIL_LIGHT_TEXT_PRIMARY)), 0);
    lv_obj_set_pos(title_label, 12, bsp_lcd_is_landscape() ? 9 : 16);
    return root;
}

lv_obj_t *ha_history_chart_create(
    lv_obj_t *parent,
    const ha_history_chart_config_t *config)
{
    if (!parent || !config) {
        return NULL;
    }

    lv_obj_t *root = history_chart_create_root(parent, config->title);
    if (!root) {
        return NULL;
    }
    if (config->message && config->message[0]) {
        history_chart_render_message(root, config->message);
        return root;
    }
    if (!config->points || config->point_count == 0 ||
        config->point_count > SIZE_MAX / sizeof(ha_history_chart_point_t) ||
        config->end_time <= config->start_time ||
        (uint64_t)(config->end_time - config->start_time) > INT32_MAX) {
        history_chart_render_message(root, i18n_get(I18N_HISTORY_EMPTY));
        return root;
    }
#if SIZE_MAX > UINT32_MAX
    if (config->point_count > UINT32_MAX) {
        history_chart_render_message(root, i18n_get(I18N_HISTORY_EMPTY));
        return root;
    }
#endif
    for (size_t i = 0; i < config->point_count; i++) {
        if (!isfinite(config->points[i].value) ||
            (config->statistics && config->points[i].has_band &&
             (!isfinite(config->points[i].lower) ||
              !isfinite(config->points[i].upper)))) {
            history_chart_render_message(root, i18n_get(I18N_HISTORY_EMPTY));
            return root;
        }
    }

    ha_history_chart_t *history = lv_malloc(sizeof(*history));
    if (!history) {
        lv_obj_delete(root);
        return NULL;
    }
    memset(history, 0, sizeof(*history));
    history->point_count = config->point_count;
    history->statistics = config->statistics;
    history->start_time = config->start_time;
    history->end_time = config->end_time;
    snprintf(history->unit,
             sizeof(history->unit),
             "%s",
             config->unit ? config->unit : "");

    history->points = lv_malloc(config->point_count *
                                sizeof(*history->points));
    if (!history->points) {
        lv_free(history);
        lv_obj_delete(root);
        return NULL;
    }
    memcpy(history->points, config->points,
           config->point_count * sizeof(*history->points));
    if (history->statistics) {
        for (size_t i = 0; i < history->point_count; i++) {
            const ha_history_chart_point_t *point = &history->points[i];
            history->series_has_stat_state |= point->has_stat_state;
            history->series_has_stat_min |= point->has_stat_min;
            history->series_has_stat_mean |= point->has_stat_mean;
            history->series_has_stat_max |= point->has_stat_max;
        }
    }
    lv_obj_add_event_cb(root,
                        history_chart_delete_cb,
                        LV_EVENT_DELETE,
                        history);

    double axis_min = 0.0;
    double axis_max = 0.0;
    size_t y_axis_tick_count = 0;
    history_chart_calculate_y_axis(config->points,
                                   config->point_count,
                                   config->statistics,
                                   &axis_min,
                                   &axis_max,
                                   &y_axis_tick_count);
    history->chart_x = history_chart_create_y_axis_labels(
        root,
        axis_min,
        axis_max,
        y_axis_tick_count);
    int32_t chart_default_width = bsp_lcd_is_landscape() ? HA_HISTORY_CHART_DEFAULT_WIDTH_LANDSCAPE : HA_HISTORY_CHART_DEFAULT_WIDTH_PORTAIT;

    history->chart_width = chart_default_width - HA_HISTORY_CHART_RIGHT - history->chart_x;
    history->axis_min = axis_min;
    history->axis_max = axis_max;
    if (history->statistics) {
        history->point_count = history_chart_lttb_reduce(
            history->points, history->point_count,
            (size_t)LV_MAX(3, history->chart_width -
                              HA_HISTORY_PLOT_PAD_LEFT -
                              HA_HISTORY_PLOT_PAD_RIGHT));
    }

    lv_obj_t *unit_label = lv_label_create(root);
    ui_i18n_label_set_text(unit_label, history->unit);
    lv_obj_set_style_text_font(unit_label, ha_ui_font_regular(16), 0);
    lv_obj_set_style_text_color(unit_label, lv_color_hex(0x666666), 0);
    lv_obj_update_layout(unit_label);
    lv_obj_set_pos(unit_label,
                    chart_default_width - HA_HISTORY_CHART_RIGHT - lv_obj_get_width(unit_label),
                   bsp_lcd_is_landscape() ? 11 : 18);

    lv_obj_t *chart = lv_chart_create(root);
    history->chart = chart;
    lv_obj_remove_style_all(chart);
    lv_obj_add_flag(chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(chart, history->chart_x, history_chart_y());
    lv_obj_set_size(chart,
                    history->chart_width,
                    history_plot_height());
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 2, 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(light_detail_theme_color(0x4D5157, LIGHT_DETAIL_LIGHT_NEUTRAL)), 0);
    lv_obj_set_style_border_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(
        chart,
        (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_BOTTOM),
        0);
    lv_obj_set_style_radius(chart, 0, 0);
    lv_obj_set_style_pad_left(chart, HA_HISTORY_PLOT_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(chart, HA_HISTORY_PLOT_PAD_RIGHT, 0);
    lv_obj_set_style_pad_top(chart, HA_HISTORY_PLOT_PAD_VER, 0);
    lv_obj_set_style_pad_bottom(chart, HA_HISTORY_PLOT_PAD_VER, 0);
    lv_obj_set_style_line_opa(chart, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart, 1, LV_PART_CURSOR);
    lv_obj_set_style_line_dash_width(chart, 4, LV_PART_CURSOR);
    lv_obj_set_style_line_dash_gap(chart, 4, LV_PART_CURSOR);
    lv_obj_set_style_width(chart, 0, LV_PART_CURSOR);
    lv_obj_set_style_height(chart, 0, LV_PART_CURSOR);

    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_point_count(chart, (uint32_t)history->point_count);
    lv_chart_set_axis_range(chart,
                            LV_CHART_AXIS_PRIMARY_X,
                            0,
                            (int32_t)(config->end_time -
                                      config->start_time));

    lv_chart_set_axis_range(chart,
                            LV_CHART_AXIS_PRIMARY_Y,
                            0,
                            HA_HISTORY_DRAW_Y_MAX);

    history->series = lv_chart_add_series(
        chart,
        lv_color_hex(history->statistics ? 0x4269d0 :
                     light_detail_theme_color(0x696D73,
                                              LIGHT_DETAIL_LIGHT_TEXT_PRIMARY)),
        (lv_chart_axis_t)(LV_CHART_AXIS_PRIMARY_Y |
                          LV_CHART_AXIS_PRIMARY_X));
    if (!history->series) {
        lv_obj_delete(root);
        return NULL;
    }

    for (size_t i = 0; i < history->point_count; i++) {
        int64_t x = (int64_t)history->points[i].epoch - config->start_time;
        x = LV_CLAMP(0, x, config->end_time - config->start_time);
        lv_chart_set_next_value2(chart,
                                 history->series,
                                 (int32_t)x,
                                 history_chart_draw_y(
                                     history->points[i].value,
                                     axis_min, axis_max));
    }

    if (history->statistics) {
        /* Rasterize the expensive band and line once. Tooltip and cursor
         * updates can then redraw the cached image without rebuilding the
         * hundreds of individual line draw tasks. */
        history_chart_cache_statistics(root, history);
        history->statistics_cursor = lv_line_create(root);
        if (history->statistics_cursor) {
            history->statistics_cursor_points[0] =
                (lv_point_precise_t){ .x = 1, .y = 0 };
            history->statistics_cursor_points[1] =
                (lv_point_precise_t){ .x = 1,
                                      .y = history_plot_height() - 1 };
            lv_line_set_points(history->statistics_cursor,
                               history->statistics_cursor_points, 2);
            lv_obj_set_size(history->statistics_cursor, 2,
                            history_plot_height());
            lv_obj_set_style_line_color(history->statistics_cursor,
                lv_color_hex(light_detail_theme_color(
                    0x454A51, LIGHT_DETAIL_LIGHT_NEUTRAL)), 0);
            lv_obj_set_style_line_width(history->statistics_cursor, 1, 0);
            lv_obj_set_style_line_dash_width(history->statistics_cursor, 4, 0);
            lv_obj_set_style_line_dash_gap(history->statistics_cursor, 4, 0);
            lv_obj_remove_flag(history->statistics_cursor,
                               LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(history->statistics_cursor, LV_OBJ_FLAG_HIDDEN);
        }
    }

    history->cursor = lv_chart_add_cursor(chart,
                                          lv_color_hex(light_detail_theme_color(0x454A51, LIGHT_DETAIL_LIGHT_NEUTRAL)),
                                          LV_DIR_VER);
    if (!history->cursor) {
        lv_obj_delete(root);
        return NULL;
    }

    history->tooltip = lv_obj_create(root);
    lv_obj_set_size(history->tooltip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(history->tooltip, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(history->tooltip, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(history->tooltip, 7, 0);
    lv_obj_set_style_pad_row(history->tooltip, 3, 0);
    lv_obj_set_style_border_width(history->tooltip, 0, 0);
    lv_obj_set_style_shadow_width(history->tooltip, 0, 0);
    lv_obj_set_style_bg_color(history->tooltip, lv_color_hex(light_detail_theme_color(0x6B6B6B, LIGHT_DETAIL_LIGHT_BG)), 0);
    lv_obj_set_style_bg_opa(history->tooltip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(history->tooltip, 8, 0);
    lv_obj_set_style_max_width(history->tooltip,
                               chart_default_width - 8, 0);
    lv_obj_remove_flag(history->tooltip,
                       LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    history->tooltip_time = lv_label_create(history->tooltip);
    lv_label_set_text(history->tooltip_time, "");
    lv_label_set_long_mode(history->tooltip_time, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_max_width(history->tooltip_time,
                               chart_default_width - 22, 0);
    lv_obj_set_style_text_font(history->tooltip_time,
                               ha_ui_font_regular(14), 0);
    lv_obj_set_style_text_color(history->tooltip_time,
        lv_color_hex(light_detail_theme_color(
            0xE5E5E5, LIGHT_DETAIL_LIGHT_TEXT_SECONDARY)), 0);

    history->tooltip_values = lv_label_create(history->tooltip);
    lv_label_set_text(history->tooltip_values, "");
    lv_label_set_long_mode(history->tooltip_values, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_max_width(history->tooltip_values,
                               chart_default_width - 22, 0);
    lv_obj_set_style_text_font(history->tooltip_values,
                               ha_ui_font_regular(16), 0);
    lv_obj_set_style_text_color(history->tooltip_values,
        lv_color_hex(light_detail_theme_color(
            0xFFFFFF, LIGHT_DETAIL_LIGHT_TEXT_PRIMARY)), 0);
    lv_obj_add_flag(history->tooltip, LV_OBJ_FLAG_HIDDEN);

    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            history->indev = indev;
            lv_indev_add_event_cb(indev,
                                  history_chart_indev_pressed_cb,
                                  LV_EVENT_PRESSED,
                                  history);
            break;
        }
        indev = lv_indev_get_next(indev);
    }

    lv_obj_add_event_cb(chart,
                        history_chart_event_cb,
                        LV_EVENT_ALL,
                        history);
    history_chart_create_time_labels(root,
                                     config->start_time,
                                     config->end_time,
                                     history->chart_x,
                                     history->chart_width);
    lv_obj_move_foreground(history->tooltip);
    lv_chart_refresh(chart);
    return root;
}
