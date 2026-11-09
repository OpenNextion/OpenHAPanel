#include "hui_trend_graph.h"

#include <math.h>
#include <string.h>

#define HUI_TREND_MAX_SAMPLES 160
#define HUI_TREND_BOTTOM_RADIUS 8

typedef struct {
    lv_obj_t *obj;
    lv_color_t color;
    size_t count;
    float x[HUI_TREND_GRAPH_MAX_POINTS];
    float from_y[HUI_TREND_GRAPH_MAX_POINTS];
    float to_y[HUI_TREND_GRAPH_MAX_POINTS];
    float display_y[HUI_TREND_GRAPH_MAX_POINTS];
} hui_trend_graph_ctx_t;

static float clamp_unit(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

static float sample_y(const hui_trend_graph_ctx_t *ctx, float x)
{
    if (!ctx->count) {
        return 0.5f;
    }
    if (x <= ctx->x[0]) {
        return ctx->display_y[0];
    }
    for (size_t i = 1; i < ctx->count; i++) {
        if (x > ctx->x[i]) {
            continue;
        }
        const float distance = ctx->x[i] - ctx->x[i - 1];
        if (distance <= 0.0f) {
            return ctx->display_y[i];
        }
        const float ratio = (x - ctx->x[i - 1]) / distance;
        return ctx->display_y[i - 1] +
               (ctx->display_y[i] - ctx->display_y[i - 1]) * ratio;
    }
    return ctx->display_y[ctx->count - 1];
}

static void graph_anim_cb(void *obj, int32_t value)
{
    hui_trend_graph_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) {
        return;
    }
    const float progress = clamp_unit((float)value / 1000.0f);
    for (size_t i = 0; i < ctx->count; i++) {
        ctx->display_y[i] = ctx->from_y[i] +
                            (ctx->to_y[i] - ctx->from_y[i]) * progress;
    }
    lv_obj_invalidate(obj);
}

static int32_t cubic_ease_out(const lv_anim_t *animation)
{
    const float progress = clamp_unit((float)lv_anim_path_linear(animation) / 1000.0f);
    const float remaining = 1.0f - progress;
    return (int32_t)lroundf((1.0f - remaining * remaining * remaining) * 1000.0f);
}

static float quadratic(float start, float control, float end, float t)
{
    const float remaining = 1.0f - t;
    return remaining * remaining * start +
           2.0f * remaining * t * control + t * t * end;
}

/* Evaluate the SVG midpoint / quadratic path at one normalized x position. */
static float graph_curve_y(const hui_trend_graph_ctx_t *ctx, float x)
{
    if (ctx->count == 1 || x <= ctx->x[0]) {
        return ctx->display_y[0];
    }
    for (size_t i = 0; i < ctx->count; i++) {
        const float start_x = i ? (ctx->x[i - 1] + ctx->x[i]) * 0.5f
                                : ctx->x[i];
        const float start_y = i ? (ctx->display_y[i - 1] +
                                    ctx->display_y[i]) * 0.5f
                                : ctx->display_y[i];
        const float end_x = i + 1 < ctx->count
                                ? (ctx->x[i] + ctx->x[i + 1]) * 0.5f
                                : ctx->x[i];
        const float end_y = i + 1 < ctx->count
                                ? (ctx->display_y[i] +
                                   ctx->display_y[i + 1]) * 0.5f
                                : ctx->display_y[i];
        if (x > end_x || end_x <= start_x) {
            continue;
        }
        float low = 0.0f;
        float high = 1.0f;
        for (int iteration = 0; iteration < 10; iteration++) {
            const float middle = (low + high) * 0.5f;
            if (quadratic(start_x, ctx->x[i], end_x, middle) < x) {
                low = middle;
            } else {
                high = middle;
            }
        }
        return clamp_unit(quadratic(start_y, ctx->display_y[i], end_y,
                                    (low + high) * 0.5f));
    }
    return ctx->display_y[ctx->count - 1];
}

static int32_t bottom_corner_inset(const lv_area_t *area, int32_t x)
{
    const int32_t radius = LV_MIN(HUI_TREND_BOTTOM_RADIUS,
                                  LV_MIN(lv_area_get_width(area),
                                         lv_area_get_height(area)) / 2);
    const int32_t edge_distance = LV_MIN(x - area->x1, area->x2 - x);
    if (edge_distance >= radius) {
        return 0;
    }
    const int32_t delta = radius - edge_distance - 1;
    return radius - (int32_t)sqrtf((float)(radius * radius -
                                           delta * delta));
}

static lv_point_precise_t clipped_line_point(const lv_area_t *area,
                                              int32_t x, int32_t y)
{
    if (y + 1 > area->y2 - bottom_corner_inset(area, x)) {
        return (lv_point_precise_t){
            .x = LV_DRAW_LINE_POINT_NONE,
            .y = LV_DRAW_LINE_POINT_NONE,
        };
    }
    return (lv_point_precise_t){ .x = x, .y = y };
}

static void append_curve_point(lv_layer_t *layer, lv_draw_line_dsc_t *line,
                               lv_point_precise_t *points, int32_t *count,
                               const lv_area_t *area, float x, float y)
{
    if (*count == HUI_TREND_MAX_SAMPLES) {
        line->points = points;
        line->point_cnt = *count;
        lv_draw_line(layer, line);
        points[0] = points[*count - 1];
        *count = 1;
    }
    points[(*count)++] = clipped_line_point(
        area, LV_CLAMP(area->x1, (int32_t)lroundf(x), area->x2),
        LV_CLAMP(area->y1, (int32_t)lroundf(y), area->y2));
}

static void draw_curve_line(lv_layer_t *layer,
                            const hui_trend_graph_ctx_t *ctx,
                            const lv_area_t *area, float top, float height)
{
    const float width = (float)(lv_area_get_width(area) - 1);
    lv_point_precise_t points[HUI_TREND_MAX_SAMPLES];
    int32_t count = 0;
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = ctx->color;
    line.width = 2;
    line.opa = 204; /* SVG stroke opacity: 0.8 */
    line.round_start = 1;
    line.round_end = 1;

    append_curve_point(layer, &line, points, &count, area,
                       (float)area->x1, top + ctx->display_y[0] * height);
    for (size_t i = 0; i < ctx->count; i++) {
        const float control_x = (float)area->x1 + ctx->x[i] * width;
        const float control_y = top + ctx->display_y[i] * height;
        const float start_x = i ? (float)area->x1 +
                                  (ctx->x[i - 1] + ctx->x[i]) * 0.5f * width
                                : control_x;
        const float start_y = i ? top +
                                  (ctx->display_y[i - 1] + ctx->display_y[i]) *
                                      0.5f * height
                                : control_y;
        const float end_x = i + 1 < ctx->count
                                ? (float)area->x1 +
                                      (ctx->x[i] + ctx->x[i + 1]) * 0.5f * width
                                : control_x;
        const float end_y = i + 1 < ctx->count
                                ? top +
                                      (ctx->display_y[i] + ctx->display_y[i + 1]) *
                                          0.5f * height
                                : control_y;
        const int32_t steps = LV_MAX(4, (int32_t)ceilf((end_x - start_x) / 3.0f));
        for (int32_t sample = 1; sample <= steps; sample++) {
            const float t = (float)sample / (float)steps;
            append_curve_point(layer, &line, points, &count, area,
                               quadratic(start_x, control_x, end_x, t),
                               quadratic(start_y, control_y, end_y, t));
        }
    }
    append_curve_point(layer, &line, points, &count, area,
                       (float)area->x2,
                       top + ctx->display_y[ctx->count - 1] * height);
    if (count > 1) {
        line.points = points;
        line.point_cnt = count;
        lv_draw_line(layer, &line);
    }
}

static void graph_event_cb(lv_event_t *event)
{
    hui_trend_graph_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        lv_anim_delete(ctx->obj, graph_anim_cb);
        lv_obj_set_user_data(ctx->obj, NULL);
        lv_free(ctx);
        return;
    }
    if (code != LV_EVENT_DRAW_MAIN || !ctx->count) {
        return;
    }
    lv_area_t area;
    lv_obj_get_coords(ctx->obj, &area);
    if (lv_area_get_width(&area) < 2 || lv_area_get_height(&area) < 5) {
        return;
    }
    const int32_t width = lv_area_get_width(&area);
    const int32_t step = LV_MAX(3, (width + HUI_TREND_MAX_SAMPLES - 2) /
                                  (HUI_TREND_MAX_SAMPLES - 1));
    const float top = (float)area.y1 + 2.0f;
    const float height = (float)lv_area_get_height(&area) - 4.0f;
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color = ctx->color;
    fill.bg_opa = 26; /* SVG area opacity: 0.1 */
    for (int32_t x = 0; x < width; x += step) {
        const float y = top + graph_curve_y(ctx, (float)x /
                                             (float)(width - 1)) * height;
        const int32_t draw_y = LV_CLAMP(area.y1, (int32_t)lroundf(y), area.y2);
        lv_area_t column = {
            .x1 = area.x1 + x,
            .y1 = draw_y,
            .x2 = LV_MIN(area.x1 + x + step - 1, area.x2),
            .y2 = area.y2,
        };
        if (column.x1 < area.x1 + HUI_TREND_BOTTOM_RADIUS ||
            column.x2 > area.x2 - HUI_TREND_BOTTOM_RADIUS) {
            const int32_t column_end = column.x2;
            for (int32_t pixel_x = column.x1; pixel_x <= column_end;
                 pixel_x++) {
                const float corner_y = top +
                    graph_curve_y(ctx,
                                  (float)(pixel_x - area.x1) /
                                  (float)(width - 1)) * height;
                column.x1 = pixel_x;
                column.x2 = pixel_x;
                column.y1 = LV_CLAMP(area.y1,
                                     (int32_t)lroundf(corner_y), area.y2);
                column.y2 = area.y2 - bottom_corner_inset(&area, pixel_x);
                if (column.y1 <= column.y2) {
                    lv_draw_rect(lv_event_get_layer(event), &fill, &column);
                }
            }
        } else {
            lv_draw_rect(lv_event_get_layer(event), &fill, &column);
        }
    }
    draw_curve_line(lv_event_get_layer(event), ctx, &area, top, height);
}

lv_obj_t *hui_trend_graph_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }
    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) {
        return NULL;
    }
    hui_trend_graph_ctx_t *ctx = lv_malloc(sizeof(*ctx));
    if (!ctx) {
        lv_obj_delete(obj);
        return NULL;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->obj = obj;
    ctx->color = lv_color_hex(0x44739E);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(obj, LV_PCT(100), 62);
    lv_obj_set_user_data(obj, ctx);
    lv_obj_add_event_cb(obj, graph_event_cb, LV_EVENT_ALL, ctx);
    return obj;
}

void hui_trend_graph_set_color(lv_obj_t *graph, lv_color_t color)
{
    hui_trend_graph_ctx_t *ctx = graph ? lv_obj_get_user_data(graph) : NULL;
    if (ctx && lv_color_to_u16(ctx->color) != lv_color_to_u16(color)) {
        ctx->color = color;
        lv_obj_invalidate(graph);
    }
}

void hui_trend_graph_set_points(lv_obj_t *graph,
                                const hui_trend_graph_point_t *points,
                                size_t count,
                                time_t start_time,
                                time_t end_time,
                                bool animate)
{
    hui_trend_graph_ctx_t *ctx = graph ? lv_obj_get_user_data(graph) : NULL;
    if (!ctx || (count && !points) || count > HUI_TREND_GRAPH_MAX_POINTS ||
        end_time <= start_time) {
        return;
    }
    lv_anim_delete(graph, graph_anim_cb);
    if (!count) {
        ctx->count = 0;
        lv_obj_invalidate(graph);
        return;
    }

    double minimum = INFINITY;
    double maximum = -INFINITY;
    for (size_t i = 0; i < count; i++) {
        if (!isfinite(points[i].value)) {
            return;
        }
        minimum = fmin(minimum, points[i].value);
        maximum = fmax(maximum, points[i].value);
    }
    const double range = maximum - minimum;
    const double margin = (range > 0.0 ? range : fmax(fabs(minimum) * 0.1, 1.0)) * 0.1;
    minimum -= margin;
    maximum += margin;
    if (minimum > 0.0) {
        minimum = fmax(0.0, minimum);
    }
    const double denominator = maximum - minimum;
    const bool had_points = ctx->count > 0;
    float next_x[HUI_TREND_GRAPH_MAX_POINTS];
    float next_y[HUI_TREND_GRAPH_MAX_POINTS];
    float previous_y[HUI_TREND_GRAPH_MAX_POINTS];
    for (size_t i = 0; i < count; i++) {
        next_x[i] = clamp_unit((float)((double)(points[i].epoch - start_time) /
                                       (double)(end_time - start_time)));
        next_y[i] = clamp_unit((float)(1.0 -
                         (points[i].value - minimum) / denominator));
        previous_y[i] = had_points ? sample_y(ctx, next_x[i]) : next_y[i];
    }
    ctx->count = count;
    for (size_t i = 0; i < count; i++) {
        ctx->x[i] = next_x[i];
        ctx->from_y[i] = previous_y[i];
        ctx->to_y[i] = next_y[i];
        ctx->display_y[i] = animate && had_points ? previous_y[i] : next_y[i];
    }
    lv_obj_invalidate(graph);
    if (animate && had_points) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, graph);
        lv_anim_set_exec_cb(&animation, graph_anim_cb);
        lv_anim_set_values(&animation, 0, 1000);
        lv_anim_set_duration(&animation, 500);
        lv_anim_set_path_cb(&animation, cubic_ease_out);
        lv_anim_start(&animation);
    }
}
