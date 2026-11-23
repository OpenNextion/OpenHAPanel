#include "hui_trend_graph.h"

#include <math.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "screen_theme.h"

#define HUI_TREND_MAX_SAMPLES 160
#define HUI_TREND_BOTTOM_RADIUS 8
#define HUI_TREND_FILL_OPA_LIGHT 26
#define HUI_TREND_FILL_OPA_DARK 64

typedef struct {
    lv_obj_t *obj;
    lv_obj_t *canvas;
    void *pixels;
    int32_t cache_width;
    int32_t cache_height;
    bool cache_ready;
    bool cache_queued;
    bool animating;
    bool dark_fill;
    lv_color_t color;
    float fill_origin;
    size_t count;
    float *x;
    float *from_y;
    float *to_y;
    float *display_y;
    float *storage;
    float *scratch;
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

static void graph_draw(lv_layer_t *layer, const hui_trend_graph_ctx_t *ctx,
                       const lv_area_t *area)
{
    const int32_t width = lv_area_get_width(area);
    const int32_t step = LV_MAX(3, (width + HUI_TREND_MAX_SAMPLES - 2) /
                                  (HUI_TREND_MAX_SAMPLES - 1));
    const float top = (float)area->y1 + 2.0f;
    const float height = (float)lv_area_get_height(area) - 4.0f;
    const int32_t origin = area->y1 + (int32_t)lroundf(
        ctx->fill_origin * (lv_area_get_height(area) - 1));
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color = ctx->color;
    fill.bg_opa = ctx->dark_fill ? HUI_TREND_FILL_OPA_DARK
                                 : HUI_TREND_FILL_OPA_LIGHT;
    for (int32_t x = 0; x < width; x += step) {
        const float y = top + graph_curve_y(ctx, (float)x /
                                             (float)(width - 1)) * height;
        const int32_t draw_y = LV_CLAMP(area->y1, (int32_t)lroundf(y), area->y2);
        lv_area_t column = {
            .x1 = area->x1 + x,
            .y1 = LV_MIN(draw_y, origin),
            .x2 = LV_MIN(area->x1 + x + step - 1, area->x2),
            .y2 = LV_MAX(draw_y, origin),
        };
        if (column.x1 < area->x1 + HUI_TREND_BOTTOM_RADIUS ||
            column.x2 > area->x2 - HUI_TREND_BOTTOM_RADIUS) {
            const int32_t column_end = column.x2;
            for (int32_t pixel_x = column.x1; pixel_x <= column_end;
                 pixel_x++) {
                const float corner_y = top +
                    graph_curve_y(ctx,
                                  (float)(pixel_x - area->x1) /
                                  (float)(width - 1)) * height;
                column.x1 = pixel_x;
                column.x2 = pixel_x;
                const int32_t curve_y = LV_CLAMP(area->y1,
                    (int32_t)lroundf(corner_y), area->y2);
                column.y1 = LV_MIN(curve_y, origin);
                column.y2 = LV_MIN(LV_MAX(curve_y, origin),
                    area->y2 - bottom_corner_inset(area, pixel_x));
                if (column.y1 <= column.y2) {
                    lv_draw_rect(layer, &fill, &column);
                }
            }
        } else {
            lv_draw_rect(layer, &fill, &column);
        }
    }
    draw_curve_line(layer, ctx, area, top, height);
}

static void graph_rebuild_cache(hui_trend_graph_ctx_t *ctx)
{
    const int32_t width = lv_obj_get_width(ctx->obj);
    const int32_t height = lv_obj_get_height(ctx->obj);
    ctx->cache_ready = false;
    if (!ctx->count || width < 2 || height < 5) {
        if (ctx->canvas) lv_obj_add_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const uint32_t stride = lv_draw_buf_width_to_stride(
        width, LV_COLOR_FORMAT_ARGB8888);
    if (!stride || (size_t)stride > SIZE_MAX / (size_t)height) {
        if (ctx->canvas) lv_obj_add_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (!ctx->pixels || ctx->cache_width != width ||
        ctx->cache_height != height) {
        if (ctx->canvas) {
            lv_obj_delete(ctx->canvas);
            ctx->canvas = NULL;
        }
        heap_caps_free(ctx->pixels);
        ctx->pixels = heap_caps_malloc((size_t)stride * (size_t)height,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!ctx->pixels) return;
        ctx->canvas = lv_canvas_create(ctx->obj);
        if (!ctx->canvas) {
            heap_caps_free(ctx->pixels);
            ctx->pixels = NULL;
            return;
        }
        lv_obj_remove_style_all(ctx->canvas);
        lv_canvas_set_buffer(ctx->canvas, ctx->pixels, width, height,
                             LV_COLOR_FORMAT_ARGB8888);
        lv_obj_remove_flag(ctx->canvas,
                           LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(ctx->canvas, 0, 0);
        ctx->cache_width = width;
        ctx->cache_height = height;
    }
    lv_canvas_fill_bg(ctx->canvas, lv_color_hex(0), LV_OPA_TRANSP);
    lv_layer_t layer;
    lv_canvas_init_layer(ctx->canvas, &layer);
    const lv_area_t area = { .x1 = 0, .y1 = 0,
                             .x2 = width - 1, .y2 = height - 1 };
    graph_draw(&layer, ctx, &area);
    lv_canvas_finish_layer(ctx->canvas, &layer);
    ctx->cache_ready = true;
    lv_obj_remove_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(ctx->obj);
}

static void graph_cache_async_cb(void *user_data)
{
    hui_trend_graph_ctx_t *ctx = user_data;
    ctx->cache_queued = false;
    if (!ctx->animating) {
        graph_rebuild_cache(ctx);
    }
}

static void graph_schedule_cache(hui_trend_graph_ctx_t *ctx)
{
    if (!ctx->cache_queued &&
        lv_async_call(graph_cache_async_cb, ctx) == LV_RESULT_OK) {
        ctx->cache_queued = true;
    }
}

static void graph_anim_ready_cb(lv_anim_t *animation)
{
    lv_obj_t *obj = animation->var;
    hui_trend_graph_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (ctx) {
        ctx->animating = false;
        graph_schedule_cache(ctx);
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
        lv_async_call_cancel(graph_cache_async_cb, ctx);
        if (ctx->canvas) lv_obj_delete(ctx->canvas);
        heap_caps_free(ctx->pixels);
        heap_caps_free(ctx->storage);
        lv_obj_set_user_data(ctx->obj, NULL);
        lv_free(ctx);
        return;
    }
    if (code == LV_EVENT_SIZE_CHANGED && !ctx->animating) {
        ctx->cache_ready = false;
        if (ctx->canvas) lv_obj_add_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
        graph_schedule_cache(ctx);
        return;
    }
    if (code != LV_EVENT_DRAW_MAIN || !ctx->count || ctx->cache_ready) {
        return;
    }
    lv_area_t area;
    lv_obj_get_coords(ctx->obj, &area);
    if (lv_area_get_width(&area) < 2 || lv_area_get_height(&area) < 5) {
        return;
    }
    graph_draw(lv_event_get_layer(event), ctx, &area);
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
    ctx->storage = heap_caps_malloc(
        7 * HUI_TREND_GRAPH_MAX_POINTS * sizeof(float),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ctx->storage) {
        lv_free(ctx);
        lv_obj_delete(obj);
        return NULL;
    }
    ctx->x = ctx->storage;
    ctx->from_y = ctx->x + HUI_TREND_GRAPH_MAX_POINTS;
    ctx->to_y = ctx->from_y + HUI_TREND_GRAPH_MAX_POINTS;
    ctx->display_y = ctx->to_y + HUI_TREND_GRAPH_MAX_POINTS;
    ctx->scratch = ctx->display_y + HUI_TREND_GRAPH_MAX_POINTS;
    ctx->obj = obj;
    ctx->color = lv_color_hex(0x44739E);
    ctx->dark_fill = screen_theme_get() == SCREEN_THEME_DARK;
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
        if (ctx->animating) {
            lv_obj_invalidate(graph);
        } else {
            ctx->cache_ready = false;
            if (ctx->canvas) lv_obj_add_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
            graph_schedule_cache(ctx);
            lv_obj_invalidate(graph);
        }
    }
}

void hui_trend_graph_apply_theme(lv_obj_t *graph)
{
    hui_trend_graph_ctx_t *ctx = graph ? lv_obj_get_user_data(graph) : NULL;
    if (!ctx) return;
    const bool dark_fill = screen_theme_get() == SCREEN_THEME_DARK;
    if (ctx->dark_fill == dark_fill) return;
    ctx->dark_fill = dark_fill;
    ctx->cache_ready = false;
    if (ctx->canvas) lv_obj_add_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
    if (!ctx->animating) graph_schedule_cache(ctx);
    lv_obj_invalidate(graph);
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
    ctx->animating = false;
    if (!count) {
        ctx->count = 0;
        ctx->cache_ready = false;
        if (ctx->canvas) lv_obj_add_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
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
    /* Match calcPoints() in the Home Assistant trend graph. */
    const double range = maximum - minimum != 0.0
        ? maximum - minimum : minimum * 0.1;
    maximum += range * 0.1;
    minimum -= range * 0.1;
    if (maximum < 0.0) {
        ctx->fill_origin = 0.0f;
    } else if (minimum < 0.0) {
        ctx->fill_origin = (float)(maximum /
            (maximum - minimum != 0.0 ? maximum - minimum : 1.0));
    } else {
        minimum = fmax(0.0, minimum);
        ctx->fill_origin = 1.0f;
    }
    const double denominator = maximum - minimum != 0.0
        ? maximum - minimum : 1.0;
    const bool had_points = ctx->count > 0;
    float *next_x = ctx->scratch;
    float *next_y = next_x + HUI_TREND_GRAPH_MAX_POINTS;
    float *previous_y = next_y + HUI_TREND_GRAPH_MAX_POINTS;
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
    ctx->cache_ready = false;
    if (ctx->canvas) lv_obj_add_flag(ctx->canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(graph);
    if (animate && had_points) {
        ctx->animating = true;
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, graph);
        lv_anim_set_exec_cb(&animation, graph_anim_cb);
        lv_anim_set_values(&animation, 0, 1000);
        lv_anim_set_duration(&animation, 500);
        lv_anim_set_path_cb(&animation, cubic_ease_out);
        lv_anim_set_completed_cb(&animation, graph_anim_ready_cb);
        lv_anim_start(&animation);
    } else {
        graph_schedule_cache(ctx);
    }
}
