#include "ha_history_chart.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assets/ha_ui_assets.h"
#include "sys_time_api.h"

#define HA_HISTORY_TIME_LABEL_COUNT 5
#define HA_HISTORY_VALUE_SCALE 100.0
#define HA_HISTORY_CHART_X 12
#define HA_HISTORY_CHART_Y 92
#define HA_HISTORY_PLOT_WIDTH 256
#define HA_HISTORY_PLOT_HEIGHT 184
#define HA_HISTORY_PLOT_PAD_HOR 28
#define HA_HISTORY_UNIT_MAX 32

typedef struct {
    lv_obj_t *chart;
    lv_obj_t *tooltip;
    lv_indev_t *indev;
    lv_chart_series_t *series;
    lv_chart_cursor_t *cursor;
    double *values;
    size_t point_count;
    char unit[HA_HISTORY_UNIT_MAX];
} ha_history_chart_t;

static void history_chart_event_cb(lv_event_t *event);
static void history_chart_indev_pressed_cb(lv_event_t *event);
static void history_chart_delete_cb(lv_event_t *event);

static int32_t history_chart_scaled_value(double value)
{
    if (value >= (double)INT32_MAX / HA_HISTORY_VALUE_SCALE) {
        return INT32_MAX;
    }
    if (value <= (double)INT32_MIN / HA_HISTORY_VALUE_SCALE) {
        return INT32_MIN;
    }
    return (int32_t)llround(value * HA_HISTORY_VALUE_SCALE);
}

static int32_t history_chart_clamp_i64(int64_t value)
{
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

static void history_chart_render_message(lv_obj_t *root, const char *text)
{
    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(label, 12, 72);
}

static void history_chart_create_time_labels(
    lv_obj_t *root,
    time_t start_time,
    time_t end_time)
{
    const time_t span = end_time > start_time ? end_time - start_time : 1;
    const int32_t plot_start = HA_HISTORY_CHART_X +
                               HA_HISTORY_PLOT_PAD_HOR;
    const int32_t plot_width = HA_HISTORY_PLOT_WIDTH -
                               HA_HISTORY_PLOT_PAD_HOR * 2;

    for (size_t i = 0; i < HA_HISTORY_TIME_LABEL_COUNT; i++) {
        const time_t epoch = start_time +
                             (time_t)((int64_t)span * i /
                                      (HA_HISTORY_TIME_LABEL_COUNT - 1));
        char time_text[6] = "--:--";
        char local_iso[40] = {0};
        if (sys_time_format_iso8601_time_by_utc_ms(
                (uint64_t)epoch * 1000,
                local_iso,
                sizeof(local_iso)) == 0 &&
            strlen(local_iso) >= 16) {
            snprintf(time_text, sizeof(time_text), "%.5s", local_iso + 11);
        }

        lv_obj_t *label = lv_label_create(root);
        lv_label_set_text(label, time_text);
        lv_obj_set_style_text_font(label, &ha_font_misan_regular_12, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
        lv_obj_update_layout(label);
        const int32_t center_x = plot_start +
                                 (int32_t)(plot_width * i /
                                           (HA_HISTORY_TIME_LABEL_COUNT - 1));
        lv_obj_set_pos(label,
                       center_x - lv_obj_get_width(label) / 2,
                       280);
    }
}

static void history_chart_draw_points(lv_event_t *event,
                                      ha_history_chart_t *history)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    if (!layer || !history || !history->chart || !history->series) {
        return;
    }

    lv_area_t chart_coords;
    lv_obj_get_coords(history->chart, &chart_coords);
    lv_draw_rect_dsc_t point_dsc;
    lv_draw_rect_dsc_init(&point_dsc);
    point_dsc.bg_color = lv_color_white();
    point_dsc.bg_opa = LV_OPA_COVER;
    point_dsc.radius = LV_RADIUS_CIRCLE;

    for (size_t i = 0; i < history->point_count; i++) {
        lv_point_t point;
        lv_chart_get_point_pos_by_id(history->chart,
                                     history->series,
                                     (uint32_t)i,
                                     &point);
        lv_area_t area = {
            .x1 = chart_coords.x1 + point.x - 3,
            .y1 = chart_coords.y1 + point.y - 3,
            .x2 = chart_coords.x1 + point.x + 3,
            .y2 = chart_coords.y1 + point.y + 3,
        };
        lv_draw_rect(layer, &point_dsc, &area);
    }
}

static void history_chart_event_cb(lv_event_t *event)
{
    ha_history_chart_t *history = lv_event_get_user_data(event);
    if (!history) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DRAW_POST_END) {
        history_chart_draw_points(event, history);
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

    lv_chart_set_cursor_point(history->chart,
                              history->cursor,
                              history->series,
                              point_id);

    char value_text[32] = {0};
    snprintf(value_text,
             sizeof(value_text),
             "%.2f",
             history->values[point_id]);
    size_t value_length = strlen(value_text);
    if (value_length > 1 && value_text[value_length - 1] == '0') {
        value_text[--value_length] = '\0';
    }

    char tooltip_text[sizeof(value_text) + HA_HISTORY_UNIT_MAX] = {0};
    snprintf(tooltip_text,
             sizeof(tooltip_text),
             "%s%s",
             value_text,
             history->unit);
    lv_label_set_text(history->tooltip, tooltip_text);
    lv_obj_remove_flag(history->tooltip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(history->tooltip);

    lv_point_t point;
    lv_chart_get_point_pos_by_id(history->chart,
                                 history->series,
                                 point_id,
                                 &point);
    const int32_t tooltip_width = lv_obj_get_width(history->tooltip);
    const int32_t tooltip_height = lv_obj_get_height(history->tooltip);
    int32_t tooltip_x = HA_HISTORY_CHART_X + point.x + 8;
    if (tooltip_x + tooltip_width > HA_HISTORY_CHART_DEFAULT_WIDTH - 6) {
        tooltip_x = HA_HISTORY_CHART_X + point.x - tooltip_width - 8;
    }
    tooltip_x = LV_CLAMP(4,
                         tooltip_x,
                         HA_HISTORY_CHART_DEFAULT_WIDTH - tooltip_width - 4);

    int32_t tooltip_y = HA_HISTORY_CHART_Y + point.y -
                        tooltip_height / 2;
    tooltip_y = LV_CLAMP(HA_HISTORY_CHART_Y,
                         tooltip_y,
                         HA_HISTORY_CHART_Y +
                             HA_HISTORY_PLOT_HEIGHT - tooltip_height);
    lv_obj_set_pos(history->tooltip, tooltip_x, tooltip_y);
    lv_obj_move_foreground(history->tooltip);
    lv_obj_invalidate(history->chart);
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
    lv_chart_set_cursor_point(history->chart,
                              history->cursor,
                              history->series,
                              LV_CHART_POINT_NONE);
    lv_obj_invalidate(history->chart);
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
    lv_free(history->values);
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
    lv_obj_set_size(root,
                    HA_HISTORY_CHART_DEFAULT_WIDTH,
                    HA_HISTORY_CHART_DEFAULT_HEIGHT);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x1E2228), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root, 12, 0);

    lv_obj_t *title_label = lv_label_create(root);
    lv_obj_set_style_text_font(title_label, &ha_font_misan_medium_20, 0);
    lv_label_set_text(title_label, title ? title : "History");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_pos(title_label, 12, 16);
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
        config->point_count > SIZE_MAX / sizeof(double) ||
        config->end_time <= config->start_time ||
        (uint64_t)(config->end_time - config->start_time) > INT32_MAX) {
        history_chart_render_message(root, "No history");
        return root;
    }
#if SIZE_MAX > UINT32_MAX
    if (config->point_count > UINT32_MAX) {
        history_chart_render_message(root, "No history");
        return root;
    }
#endif
    for (size_t i = 0; i < config->point_count; i++) {
        if (!isfinite(config->points[i].value)) {
            history_chart_render_message(root, "No history");
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
    snprintf(history->unit,
             sizeof(history->unit),
             "%s",
             config->unit ? config->unit : "");

    history->values = lv_malloc(config->point_count *
                                sizeof(*history->values));
    if (!history->values) {
        lv_free(history);
        lv_obj_delete(root);
        return NULL;
    }
    for (size_t i = 0; i < config->point_count; i++) {
        history->values[i] = config->points[i].value;
    }
    lv_obj_add_event_cb(root,
                        history_chart_delete_cb,
                        LV_EVENT_DELETE,
                        history);

    lv_obj_t *unit_label = lv_label_create(root);
    lv_label_set_text(unit_label, history->unit);
    lv_obj_set_style_text_font(unit_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(unit_label, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(unit_label, 12, 62);

    lv_obj_t *chart = lv_chart_create(root);
    history->chart = chart;
    lv_obj_remove_style_all(chart);
    lv_obj_add_flag(chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(chart, HA_HISTORY_CHART_X, HA_HISTORY_CHART_Y);
    lv_obj_set_size(chart, HA_HISTORY_PLOT_WIDTH, HA_HISTORY_PLOT_HEIGHT);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 2, 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x4D5157), 0);
    lv_obj_set_style_border_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(
        chart,
        (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_BOTTOM),
        0);
    lv_obj_set_style_radius(chart, 0, 0);
    lv_obj_set_style_pad_left(chart, HA_HISTORY_PLOT_PAD_HOR, 0);
    lv_obj_set_style_pad_right(chart, HA_HISTORY_PLOT_PAD_HOR, 0);
    lv_obj_set_style_pad_top(chart, 12, 0);
    lv_obj_set_style_pad_bottom(chart, 12, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart, 6, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 6, LV_PART_INDICATOR);
    lv_obj_set_style_radius(chart, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart, 1, LV_PART_CURSOR);
    lv_obj_set_style_line_dash_width(chart, 4, LV_PART_CURSOR);
    lv_obj_set_style_line_dash_gap(chart, 4, LV_PART_CURSOR);
    lv_obj_set_style_width(chart, 0, LV_PART_CURSOR);
    lv_obj_set_style_height(chart, 0, LV_PART_CURSOR);

    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_point_count(chart, (uint32_t)config->point_count);
    lv_chart_set_axis_range(chart,
                            LV_CHART_AXIS_PRIMARY_X,
                            0,
                            (int32_t)(config->end_time -
                                      config->start_time));

    int32_t min_value = history_chart_scaled_value(config->points[0].value);
    int32_t max_value = min_value;
    for (size_t i = 1; i < config->point_count; i++) {
        const int32_t value = history_chart_scaled_value(
            config->points[i].value);
        min_value = LV_MIN(min_value, value);
        max_value = LV_MAX(max_value, value);
    }
    const int64_t value_span = (int64_t)max_value - min_value;
    const int64_t padding = LV_MAX(value_span / 3, 100);
    int32_t axis_min = history_chart_clamp_i64((int64_t)min_value - padding);
    int32_t axis_max = history_chart_clamp_i64((int64_t)max_value + padding);
    if (axis_max <= axis_min) {
        axis_max = axis_min < INT32_MAX ? axis_min + 1 : INT32_MAX;
    }
    lv_chart_set_axis_range(chart,
                            LV_CHART_AXIS_PRIMARY_Y,
                            axis_min,
                            axis_max);

    history->series = lv_chart_add_series(
        chart,
        lv_color_hex(0x696D73),
        (lv_chart_axis_t)(LV_CHART_AXIS_PRIMARY_Y |
                          LV_CHART_AXIS_PRIMARY_X));
    if (!history->series) {
        lv_obj_delete(root);
        return NULL;
    }

    for (size_t i = 0; i < config->point_count; i++) {
        int64_t x = (int64_t)config->points[i].epoch - config->start_time;
        x = LV_CLAMP(0, x, config->end_time - config->start_time);
        lv_chart_set_next_value2(chart,
                                 history->series,
                                 (int32_t)x,
                                 history_chart_scaled_value(
                                     config->points[i].value));
    }

    history->cursor = lv_chart_add_cursor(chart,
                                          lv_color_hex(0x454A51),
                                          LV_DIR_VER);
    if (!history->cursor) {
        lv_obj_delete(root);
        return NULL;
    }

    history->tooltip = lv_label_create(root);
    lv_label_set_text(history->tooltip, "");
    lv_obj_set_size(history->tooltip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(history->tooltip,
                               &ha_font_misan_regular_16,
                               0);
    lv_obj_set_style_text_color(history->tooltip, lv_color_white(), 0);
    lv_obj_set_style_bg_color(history->tooltip, lv_color_hex(0x6B6B6B), 0);
    lv_obj_set_style_bg_opa(history->tooltip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(history->tooltip, 8, 0);
    lv_obj_set_style_pad_hor(history->tooltip, 6, 0);
    lv_obj_set_style_pad_ver(history->tooltip, 5, 0);
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
                                     config->end_time);
    lv_chart_refresh(chart);
    return root;
}
