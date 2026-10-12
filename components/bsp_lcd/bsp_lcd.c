#include "driver/i2c_master.h"
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
#include "bsp_lcd.h"
#include "exio_pcf8574.h"
#include "lcd_st7796u.h"
#include "lcd_touch_cst826.h"

#define LVGL_TASK_STACK_SIZE (16 * 1024)
#define LVGL_EXTRA_MEMORY_POOL_COUNT 2U
#define LVGL_EXTRA_MEMORY_POOL_SIZE  (64U * 1024U)

static esp_err_t bsp_lcd_i2c_init(void);
static esp_err_t bsp_lcd_spi_init(void);
static esp_err_t bsp_lcd_br_init(void);
esp_err_t bsp_lcd_br_set(uint8_t br);
static esp_err_t bsp_lcd_touch_init(void);
static esp_err_t bsp_lcd_lvgl_init(void);
static esp_err_t bsp_lcd_lvgl_add_extra_memory_pool(void);

static const char *TAG = "bsp_lcd";

static i2c_master_bus_handle_t s_i2c_0_bus_handle = NULL;

static esp_lcd_panel_io_handle_t s_lcd_io_handle = NULL;

static esp_lcd_panel_handle_t s_lcd_panel_handle = NULL;

static esp_lcd_touch_handle_t s_lcd_touch_handle = NULL;

static lv_display_t *s_lvgl_display = NULL;

static lv_indev_t *s_lvgl_touch = NULL;

static void *s_lvgl_extra_memory_pools[LVGL_EXTRA_MEMORY_POOL_COUNT];

esp_err_t bsp_lcd_init(void)
{
    ESP_ERROR_CHECK(bsp_lcd_i2c_init());
    ESP_ERROR_CHECK(exio_pcf8574_init());
    ESP_ERROR_CHECK(bsp_lcd_br_init());
    ESP_ERROR_CHECK(bsp_lcd_br_set(0));
    ESP_ERROR_CHECK(bsp_lcd_spi_init());
    ESP_ERROR_CHECK(bsp_lcd_touch_init());
    ESP_ERROR_CHECK(bsp_lcd_lvgl_init());

    /* Keep the backlight off until the application has rendered its first frame. */

    ESP_LOGI(TAG, "Init BSP LCD success");
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

    ESP_LOGI(TAG, "Set LCD brightness to %d%%", br);
    return ESP_OK;
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
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_conf), TAG, "Init LVGL port failed");
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
    s_lvgl_display = lvgl_port_add_disp(&disp_conf);
    if (s_lvgl_display == NULL) {
        ESP_LOGE(TAG, "Init LCD LVGL display failed");
        return ESP_FAIL;
    }

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
