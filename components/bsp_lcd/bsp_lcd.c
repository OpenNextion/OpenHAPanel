#include <inttypes.h>

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_disp.h"
#include "esp_lvgl_port_touch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_lcd.h"
#include "exio_pcf8574.h"
#include "lcd_st7796u.h"
#include "lcd_touch_cst826.h"

#define LVGL_TASK_STACK_SIZE (12 * 1024)
#define LVGL_EXTRA_MEMORY_POOL_COUNT 2U
#define LVGL_EXTRA_MEMORY_POOL_SIZE  (256U * 1024U)
#define BOOT_TEXT_SCALE 3
#define BOOT_GLYPH_WIDTH 5
#define BOOT_GLYPH_HEIGHT 7
#define BOOT_GLYPH_ADVANCE 6
#define BOOT_TEXT_LENGTH 11

static esp_err_t bsp_lcd_i2c_init(void);
static esp_err_t bsp_lcd_spi_init(void);
static esp_err_t bsp_lcd_br_init(void);
esp_err_t bsp_lcd_br_set(uint8_t br);
static esp_err_t bsp_lcd_touch_init(void);
static esp_err_t bsp_lcd_lvgl_init(void);
static esp_err_t bsp_lcd_lvgl_add_extra_memory_pool(void);
static void bsp_lcd_lvgl_flush_cb(lv_display_t *display,
                                  const lv_area_t *area,
                                  uint8_t *color_map);

static const char *TAG = "bsp_lcd";
static volatile uint8_t s_applied_brightness;
static bsp_lcd_rotation_t s_applied_rotation = BSP_LCD_ROTATION_0;

static i2c_master_bus_handle_t s_i2c_0_bus_handle = NULL;

static esp_lcd_panel_io_handle_t s_lcd_io_handle = NULL;

static esp_lcd_panel_handle_t s_lcd_panel_handle = NULL;

static esp_lcd_touch_handle_t s_lcd_touch_handle = NULL;

static lv_display_t *s_lvgl_display = NULL;

static lv_indev_t *s_lvgl_touch = NULL;

static void *s_lvgl_extra_memory_pools[LVGL_EXTRA_MEMORY_POOL_COUNT];
static bool s_lcd_flush_faulted;
static bool s_lcd_hardware_ready;
static bool s_boot_screen_active;

/* 5x7 bitmap for "OpenHAPanel"; independent of LVGL and the TTF file. */
static const uint8_t s_boot_text[BOOT_TEXT_LENGTH][BOOT_GLYPH_HEIGHT] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x00, 0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10}, /* p */
    {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E}, /* e */
    {0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11}, /* n */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F}, /* a */
    {0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11}, /* n */
    {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E}, /* e */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0E}, /* l */
};

static esp_err_t bsp_lcd_boot_screen_draw(void)
{
    const size_t pixel_count = LCD_H_RES * LCD_FLUSH_LINES;
    uint16_t *pixels = heap_caps_aligned_alloc(64, pixel_count * sizeof(*pixels),
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!pixels) return ESP_ERR_NO_MEM;

    const int text_width = (BOOT_TEXT_LENGTH * BOOT_GLYPH_ADVANCE - 1) * BOOT_TEXT_SCALE;
    const int text_height = BOOT_GLYPH_HEIGHT * BOOT_TEXT_SCALE;
    const int text_x = (LCD_H_RES - text_width) / 2;
    const int text_y = (LCD_V_RES - text_height) / 2;
    /* RGB565 rounds 0x010203 to black on this 16-bit panel. */
    const uint16_t background = 0x0000;
    const uint16_t foreground = 0xFFFF;
    esp_err_t err = ESP_OK;

    for (int top = 0; top < LCD_V_RES; top += LCD_FLUSH_LINES) {
        for (int row = 0; row < LCD_FLUSH_LINES; ++row) {
            const int y = top + row - text_y;
            for (int x = 0; x < LCD_H_RES; ++x) {
                bool text_pixel = false;
                const int tx = x - text_x;
                if (y >= 0 && y < text_height && tx >= 0 && tx < text_width) {
                    const int glyph = tx / (BOOT_GLYPH_ADVANCE * BOOT_TEXT_SCALE);
                    const int column = (tx / BOOT_TEXT_SCALE) % BOOT_GLYPH_ADVANCE;
                    if (column < BOOT_GLYPH_WIDTH) {
                        text_pixel = (s_boot_text[glyph][y / BOOT_TEXT_SCALE] &
                                      (1U << (BOOT_GLYPH_WIDTH - 1 - column))) != 0;
                    }
                }
                pixels[row * LCD_H_RES + x] = text_pixel ? foreground : background;
            }
        }
        err = esp_lcd_panel_draw_bitmap(s_lcd_panel_handle, 0, top,
                                        LCD_H_RES, top + LCD_FLUSH_LINES, pixels);
        if (err != ESP_OK) break;
        /* draw_bitmap queues DMA. Wait before rewriting this shared strip. */
        err = esp_lcd_panel_io_tx_param(s_lcd_io_handle, -1, NULL, 0);
        if (err != ESP_OK) break;
    }
    /* Also drain a transaction if a strip failed after being queued. */
    esp_err_t wait_err = esp_lcd_panel_io_tx_param(s_lcd_io_handle, -1, NULL, 0);
    if (err == ESP_OK) err = wait_err;
    heap_caps_free(pixels);
    return err;
}

esp_err_t bsp_lcd_boot_screen_show(void)
{
    /* The panel may retain its old GRAM contents across an ESP reset. Drive
     * the backlight low before any slower peripheral initialization. */
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_BL_PIN, 0), TAG, "Set backlight low failed");
    ESP_RETURN_ON_ERROR(gpio_set_direction(LCD_BL_PIN, GPIO_MODE_OUTPUT),
                        TAG, "Disable backlight GPIO failed");
    if (!s_lcd_hardware_ready) {
        ESP_RETURN_ON_ERROR(bsp_lcd_i2c_init(), TAG, "Init boot screen I2C failed");
        ESP_RETURN_ON_ERROR(exio_pcf8574_init(), TAG, "Init boot screen IO expander failed");
        ESP_RETURN_ON_ERROR(bsp_lcd_br_init(), TAG, "Init boot screen backlight failed");
        ESP_RETURN_ON_ERROR(bsp_lcd_br_set(0), TAG, "Disable boot screen backlight failed");
        ESP_RETURN_ON_ERROR(bsp_lcd_spi_init(), TAG, "Init boot screen LCD failed");
        s_lcd_hardware_ready = true;
    }
    ESP_RETURN_ON_ERROR(bsp_lcd_boot_screen_draw(), TAG, "Draw boot screen failed");
    /* SPI completion does not mean the panel has scanned the new GRAM data.
     * Match the settle time used before enabling the backlight on master. */
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(bsp_lcd_br_set(50), TAG, "Enable boot screen backlight failed");
    s_boot_screen_active = true;
    return ESP_OK;
}

bool bsp_lcd_boot_screen_active(void)
{
    return s_boot_screen_active;
}

void bsp_lcd_boot_screen_finish(void)
{
    s_boot_screen_active = false;
}

esp_err_t bsp_lcd_init(void)
{
    if (!s_lcd_hardware_ready) {
        ESP_ERROR_CHECK(bsp_lcd_i2c_init());
        ESP_ERROR_CHECK(exio_pcf8574_init());
        ESP_ERROR_CHECK(bsp_lcd_br_init());
        ESP_ERROR_CHECK(bsp_lcd_br_set(0));
        ESP_ERROR_CHECK(bsp_lcd_spi_init());
        s_lcd_hardware_ready = true;
    }
    ESP_ERROR_CHECK(bsp_lcd_touch_init());
    ESP_ERROR_CHECK(bsp_lcd_lvgl_init());

    /* The native boot screen keeps the backlight on until the first UI frame. */

    ESP_LOGI(TAG, "Init BSP LCD success");
    return ESP_OK;
}

esp_err_t bsp_lcd_set_rotation(bsp_lcd_rotation_t rotation)
{
    lv_display_rotation_t lv_rotation;
    switch (rotation) {
    case BSP_LCD_ROTATION_0:
        lv_rotation = LV_DISPLAY_ROTATION_0;
        break;
    case BSP_LCD_ROTATION_90:
        lv_rotation = LV_DISPLAY_ROTATION_90;
        break;
    case BSP_LCD_ROTATION_180:
        lv_rotation = LV_DISPLAY_ROTATION_180;
        break;
    case BSP_LCD_ROTATION_270:
        lv_rotation = LV_DISPLAY_ROTATION_270;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_lvgl_display) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_applied_rotation == rotation) {
        return ESP_OK;
    }

    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_display_set_rotation(s_lvgl_display, lv_rotation);
    const int32_t width = lv_display_get_horizontal_resolution(s_lvgl_display);
    const int32_t height = lv_display_get_vertical_resolution(s_lvgl_display);
    s_applied_rotation = rotation;
    lvgl_port_unlock();

    ESP_LOGI(TAG,
             "LCD rotation applied: rotation=%d logical_resolution=%" PRId32
             "x%" PRId32 " software_rotation=disabled",
             (int)rotation,
             width,
             height);
    return ESP_OK;
}

bsp_lcd_rotation_t bsp_lcd_get_rotation(void)
{
    return s_applied_rotation;
}

bsp_lcd_orientation_t bsp_lcd_get_orientation(void)
{
    switch (s_applied_rotation) {
    case BSP_LCD_ROTATION_90:
    case BSP_LCD_ROTATION_270:
        return BSP_LCD_ORIENTATION_LANDSCAPE;
    case BSP_LCD_ROTATION_0:
    case BSP_LCD_ROTATION_180:
    default:
        return BSP_LCD_ORIENTATION_PORTRAIT;
    }
}

bool bsp_lcd_is_landscape(void)
{
    return bsp_lcd_get_orientation() == BSP_LCD_ORIENTATION_LANDSCAPE;
}

esp_err_t bsp_lcd_get_logical_resolution(int32_t *width, int32_t *height)
{
    if (!width || !height) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_lvgl_display) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }

    *width = lv_display_get_horizontal_resolution(s_lvgl_display);
    *height = lv_display_get_vertical_resolution(s_lvgl_display);
    lvgl_port_unlock();
    return ESP_OK;
}

static esp_err_t bsp_lcd_i2c_init(void)
{
    i2c_master_bus_config_t bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = LCD_I2C_0_SCL_PIN,
        .sda_io_num = LCD_I2C_0_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_conf, &s_i2c_0_bus_handle), TAG, "Init I2C num 0 bus failed");
    return ESP_OK;
}

static esp_err_t bsp_lcd_spi_init(void)
{
    spi_bus_config_t bus_conf = {
        .sclk_io_num = LCD_SPI_2_SCLK_PIN,
        .mosi_io_num = LCD_SPI_2_MOSI_PIN,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * LCD_FLUSH_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus_conf, SPI_DMA_CH_AUTO), TAG, "Init SPI bus failed");

    esp_lcd_panel_io_spi_config_t io_conf = {
        .dc_gpio_num = LCD_DC_PIN,
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .flags = {
            .psram_dma_direct = true,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_conf, &s_lcd_io_handle), TAG, "Init SPI panel IO failed");

    esp_lcd_panel_dev_config_t panel_conf = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796u(s_lcd_io_handle, &panel_conf, &s_lcd_panel_handle), TAG, "Init st7796u panel failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_lcd_panel_handle), TAG, "Reset LCD panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_lcd_panel_handle), TAG, "Init LCD panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_lcd_panel_handle, false), TAG, "Invert LCD panel color failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd_panel_handle, true), TAG, "Turn LCD panel ON failed");
    /* Match the LVGL display's initial mirror setting before the native boot
     * frame is drawn, so the text is not reflected on the physical panel. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_lcd_panel_handle, true, false),
                        TAG, "Set initial LCD mirror failed");

    return ESP_OK;
}

static esp_err_t bsp_lcd_br_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 10000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_conf), TAG, "Config LEDC timer failed");

    ledc_channel_config_t channel_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_1,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LCD_BL_PIN,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_conf), TAG, "Config LEDC channel failed");

    return ESP_OK;
}

esp_err_t bsp_lcd_br_set(uint8_t br)
{
    if (br > 100) {
        ESP_LOGW(TAG, "The value of br was out of range (0~100), value: %d", br);
        br = 100;
    }

    uint32_t duty = (br * (1024 - 1)) / 100;

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty), TAG, "Set LEDC duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG, "Update LEDC duty failed");

    s_applied_brightness = br;

    ESP_LOGI(TAG, "Set LCD brightness to %d%%", br);
    return ESP_OK;
}

uint8_t bsp_lcd_br_get(void)
{
    return s_applied_brightness;
}

static esp_err_t bsp_lcd_touch_init(void)
{
    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_get_bus_handle(I2C_NUM_0, &bus_handle), TAG, "Get I2C num 0 bus handle failed");

    esp_lcd_panel_io_i2c_config_t tp_io_config = {
        .dev_addr = LCD_TOUCH_CST826_I2C_ADDR,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .flags.disable_control_phase = 1,
        .scl_speed_hz = LCD_TOUCH_CST826_I2C_SPEED_HZ,
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus_handle, &tp_io_config, &tp_io_handle), TAG, "Init LCD touch IO failed");

    esp_lcd_touch_config_t tp_conf = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_cst826(tp_io_handle, &tp_conf, &s_lcd_touch_handle), TAG, "Init LCD touch cst826 failed");

    return ESP_OK;
}

static esp_err_t bsp_lcd_lvgl_init(void)
{
    lvgl_port_cfg_t port_conf = ESP_LVGL_PORT_INIT_CONFIG();
    port_conf.task_stack = LVGL_TASK_STACK_SIZE;
    /* LVGL event handlers can write NVS/flash. Their stack must stay in
     * internal RAM while the flash cache is disabled. */
    port_conf.task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    esp_err_t err = lvgl_port_init(&port_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LVGL task init failed: %s; internal free=%u largest=%u, PSRAM free=%u largest=%u",
                 esp_err_to_name(err),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        return err;
    }
    ESP_RETURN_ON_ERROR(bsp_lcd_lvgl_add_extra_memory_pool(),
                        TAG,
                        "Add LVGL PSRAM memory pool failed");

    const lvgl_port_display_cfg_t disp_conf = {
        .io_handle = s_lcd_io_handle,
        .panel_handle = s_lcd_panel_handle,
        .buffer_size = LCD_H_RES * LVGL_BUF_LINES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = false,
            .swap_bytes = true,
        },
    };
    /* add_disp creates a default white screen. Hold the recursive LVGL lock
     * through callback replacement so its task cannot flush that screen. */
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    s_lvgl_display = lvgl_port_add_disp(&disp_conf);
    if (s_lvgl_display == NULL) {
        lvgl_port_unlock();
        ESP_LOGE(TAG, "Init LCD LVGL display failed");
        return ESP_FAIL;
    }

    /* esp_lvgl_port 2.9.0 ignores esp_lcd_panel_draw_bitmap() failures. If a
     * PSRAM-direct SPI DMA transaction underflows, its completion callback is
     * not guaranteed to release the current LVGL flush and taskLVGL can remain
     * in wait_for_flushing() until the watchdog fires. Keep the port's normal
     * transfer-complete callback, but use a checked flush callback here. */
    lv_display_set_flush_cb(s_lvgl_display, bsp_lcd_lvgl_flush_cb);
    lvgl_port_unlock();

    const lvgl_port_touch_cfg_t touch_conf = {
        .disp = s_lvgl_display,
        .handle = s_lcd_touch_handle,
    };
    s_lvgl_touch = lvgl_port_add_touch(&touch_conf);
    if (s_lvgl_touch == NULL) {
        ESP_LOGE(TAG, "Init LCD LVGL touch failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void bsp_lcd_lvgl_flush_cb(lv_display_t *display,
                                  const lv_area_t *area,
                                  uint8_t *color_map)
{
    if (!display || !area || !color_map) {
        if (display) {
            lv_display_flush_ready(display);
        }
        return;
    }

    /* Keep the native startup frame visible until the application has built
     * its first normal LVGL page. */
    if (s_boot_screen_active) {
        lv_display_flush_ready(display);
        return;
    }

    /* A DMA underflow leaves esp_lcd's queued-transaction accounting unsafe to
     * reuse. Do not enter the blocking transaction recycler again. The screen
     * keeps its last valid frame while the rest of the system (including OTA)
     * remains alive and can complete or reboot normally. */
    if (s_lcd_flush_faulted) {
        lv_display_flush_ready(display);
        return;
    }

    lv_draw_sw_rgb565_swap(color_map, lv_area_get_size(area));
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_lcd_panel_handle,
                                               area->x1,
                                               area->y1,
                                               area->x2 + 1,
                                               area->y2 + 1,
                                               color_map);
    if (err != ESP_OK) {
        s_lcd_flush_faulted = true;
        ESP_LOGE(TAG,
                 "LCD flush failed: %s; further flushes disabled until reboot",
                 esp_err_to_name(err));
        lv_display_flush_ready(display);
    }
}

static esp_err_t bsp_lcd_lvgl_add_extra_memory_pool(void)
{
    if (s_lvgl_extra_memory_pools[0]) {
        return ESP_OK;
    }

    void *memory[LVGL_EXTRA_MEMORY_POOL_COUNT] = {0};
    for (size_t i = 0; i < LVGL_EXTRA_MEMORY_POOL_COUNT; i++) {
        memory[i] = heap_caps_malloc(LVGL_EXTRA_MEMORY_POOL_SIZE,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!memory[i]) {
            for (size_t j = 0; j < i; j++) {
                heap_caps_free(memory[j]);
            }
            ESP_LOGE(TAG,
                     "Failed to allocate LVGL PSRAM pool %u/%u",
                     (unsigned)(i + 1),
                     (unsigned)LVGL_EXTRA_MEMORY_POOL_COUNT);
            return ESP_ERR_NO_MEM;
        }
    }

    if (!lvgl_port_lock(0)) {
        for (size_t i = 0; i < LVGL_EXTRA_MEMORY_POOL_COUNT; i++) {
            heap_caps_free(memory[i]);
        }
        return ESP_ERR_TIMEOUT;
    }

    lv_mem_pool_t pools[LVGL_EXTRA_MEMORY_POOL_COUNT] = {0};
    size_t added = 0;
    for (; added < LVGL_EXTRA_MEMORY_POOL_COUNT; added++) {
        pools[added] = lv_mem_add_pool(memory[added],
                                       LVGL_EXTRA_MEMORY_POOL_SIZE);
        if (!pools[added]) {
            break;
        }
    }

    lv_mem_monitor_t monitor = {0};
    if (added == LVGL_EXTRA_MEMORY_POOL_COUNT) {
        for (size_t i = 0; i < LVGL_EXTRA_MEMORY_POOL_COUNT; i++) {
            s_lvgl_extra_memory_pools[i] = memory[i];
        }
        lv_mem_monitor(&monitor);
    }
    else {
        for (size_t i = added; i > 0; i--) {
            lv_mem_remove_pool(pools[i - 1]);
        }
    }
    lvgl_port_unlock();

    if (added != LVGL_EXTRA_MEMORY_POOL_COUNT) {
        for (size_t i = 0; i < LVGL_EXTRA_MEMORY_POOL_COUNT; i++) {
            heap_caps_free(memory[i]);
        }
        ESP_LOGE(TAG,
                 "LVGL rejected PSRAM pool %u/%u",
                 (unsigned)(added + 1),
                 (unsigned)LVGL_EXTRA_MEMORY_POOL_COUNT);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "LVGL memory expanded with %u PSRAM pools: total=%u free=%u largest=%u",
             (unsigned)LVGL_EXTRA_MEMORY_POOL_COUNT,
             (unsigned)monitor.total_size,
             (unsigned)monitor.free_size,
             (unsigned)monitor.free_biggest_size);
    return ESP_OK;
}
