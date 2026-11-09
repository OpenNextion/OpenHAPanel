#include "font_render_worker.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define FONT_WORKER_STACK_SIZE (24 * 1024)

typedef enum {
    FONT_REQUEST_GLYPH_DSC,
    FONT_REQUEST_GLYPH_BITMAP,
} font_request_type_t;

typedef struct {
    font_request_type_t type;
    const lv_font_t *font;
    lv_font_glyph_dsc_t *glyph;
    lv_draw_buf_t *draw_buf;
    uint32_t letter;
    uint32_t letter_next;
    bool dsc_result;
    const void *bitmap_result;
} font_request_t;

static const char *TAG = "font_worker";
static bool (*s_original_get_glyph_dsc)(const lv_font_t *, lv_font_glyph_dsc_t *, uint32_t, uint32_t);
static const void *(*s_original_get_glyph_bitmap)(lv_font_glyph_dsc_t *, lv_draw_buf_t *);
static SemaphoreHandle_t s_request_lock;
static SemaphoreHandle_t s_request_ready;
static SemaphoreHandle_t s_request_done;
static font_request_t *s_request;
static TaskHandle_t s_worker_task;

static void font_worker_task(void *arg)
{
    (void)arg;
    bool first_request = true;
    for (;;) {
        xSemaphoreTake(s_request_ready, portMAX_DELAY);
        font_request_t *request = s_request;
        if (first_request && !heap_caps_check_integrity_all(false)) {
            ESP_LOGE(TAG, "Heap integrity failed before first FreeType glyph");
        }
        if (request->type == FONT_REQUEST_GLYPH_DSC) {
            request->dsc_result = s_original_get_glyph_dsc(request->font, request->glyph,
                                                            request->letter, request->letter_next);
        } else {
            request->bitmap_result = s_original_get_glyph_bitmap(request->glyph, request->draw_buf);
        }
        if (first_request) {
            ESP_LOGI(TAG, "First glyph complete; worker stack minimum free=%u bytes",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
            first_request = false;
        }
        xSemaphoreGive(s_request_done);
    }
}

static void font_worker_submit(font_request_t *request)
{
    static bool lvgl_stack_logged;
    if (!lvgl_stack_logged && strcmp(pcTaskGetName(NULL), "taskLVGL") == 0) {
        lvgl_stack_logged = true;
        ESP_LOGI(TAG, "LVGL stack minimum free before glyph=%u bytes",
                 (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
    xSemaphoreTake(s_request_lock, portMAX_DELAY);
    s_request = request;
    xSemaphoreGive(s_request_ready);
    xSemaphoreTake(s_request_done, portMAX_DELAY);
    s_request = NULL;
    xSemaphoreGive(s_request_lock);
}

static bool font_worker_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *glyph,
                                      uint32_t letter, uint32_t letter_next)
{
    font_request_t request = {
        .type = FONT_REQUEST_GLYPH_DSC,
        .font = font,
        .glyph = glyph,
        .letter = letter,
        .letter_next = letter_next,
    };
    font_worker_submit(&request);
    /* FreeType reports glyph index 0 as a found placeholder. Treat it as
     * missing so LVGL draws its configured square placeholder instead. */
    return request.dsc_result && !glyph->is_placeholder;
}

static const void *font_worker_get_glyph_bitmap(lv_font_glyph_dsc_t *glyph,
                                                 lv_draw_buf_t *draw_buf)
{
    font_request_t request = {
        .type = FONT_REQUEST_GLYPH_BITMAP,
        .glyph = glyph,
        .draw_buf = draw_buf,
    };
    font_worker_submit(&request);
    return request.bitmap_result;
}

bool ha_font_render_worker_attach(lv_font_t *font)
{
    if (!font) return false;

    if (s_worker_task) {
        if (font->get_glyph_dsc != s_original_get_glyph_dsc ||
            font->get_glyph_bitmap != s_original_get_glyph_bitmap) return false;
        font->get_glyph_dsc = font_worker_get_glyph_dsc;
        font->get_glyph_bitmap = font_worker_get_glyph_bitmap;
        return true;
    }

    s_request_lock = xSemaphoreCreateMutex();
    s_request_ready = xSemaphoreCreateBinary();
    s_request_done = xSemaphoreCreateBinary();
    if (!s_request_lock || !s_request_ready || !s_request_done) goto failed;

    s_original_get_glyph_dsc = font->get_glyph_dsc;
    s_original_get_glyph_bitmap = font->get_glyph_bitmap;
    if (!s_original_get_glyph_dsc || !s_original_get_glyph_bitmap) goto failed;

    if (xTaskCreateWithCaps(font_worker_task, "fontWorker", FONT_WORKER_STACK_SIZE,
                            NULL, 5, &s_worker_task,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) goto failed;

    font->get_glyph_dsc = font_worker_get_glyph_dsc;
    font->get_glyph_bitmap = font_worker_get_glyph_bitmap;
    ESP_LOGI(TAG, "FreeType glyph worker started with %u-byte PSRAM stack",
             (unsigned)FONT_WORKER_STACK_SIZE);
    return true;

failed:
    if (s_request_lock) vSemaphoreDelete(s_request_lock);
    if (s_request_ready) vSemaphoreDelete(s_request_ready);
    if (s_request_done) vSemaphoreDelete(s_request_done);
    s_request_lock = NULL;
    s_request_ready = NULL;
    s_request_done = NULL;
    ESP_LOGE(TAG, "Unable to start FreeType glyph worker");
    return false;
}
