#ifndef OPEN_NEXTION_COMPONENTS_BSP_LCD_H
#define OPEN_NEXTION_COMPONENTS_BSP_LCD_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define LCD_I2C_0_SCL_PIN       GPIO_NUM_7
#define LCD_I2C_0_SDA_PIN       GPIO_NUM_8
#define LCD_SPI_2_SCLK_PIN      GPIO_NUM_5
#define LCD_SPI_2_MOSI_PIN      GPIO_NUM_1
#define LCD_DC_PIN              GPIO_NUM_3
#define LCD_CS_PIN              GPIO_NUM_2
#define LCD_BL_PIN              GPIO_NUM_6

#define LCD_SPI_HOST            SPI2_HOST
#define LCD_H_RES               320
#define LCD_V_RES               480
/* ESP32-S3 GPSPI uses the 80 MHz APB clock with an integer divider. There is
 * no effective 60 MHz step: requesting 60 MHz resolves to 40 MHz in ESP-IDF.
 * Use the explicit safe value for PSRAM-direct LCD DMA. */
#define LCD_PCLK_HZ             (40 * 1000 * 1000)
#define LCD_FLUSH_LINES         80

#define LVGL_BUF_LINES          80

typedef enum {
    BSP_LCD_ROTATION_0 = 0,
    BSP_LCD_ROTATION_90 = 90,
    BSP_LCD_ROTATION_180 = 180,
    BSP_LCD_ROTATION_270 = 270,
} bsp_lcd_rotation_t;

typedef enum {
    BSP_LCD_ORIENTATION_PORTRAIT,
    BSP_LCD_ORIENTATION_LANDSCAPE,
} bsp_lcd_orientation_t;

esp_err_t bsp_lcd_init(void);
esp_err_t bsp_lcd_br_set(uint8_t br);
/** Last brightness successfully written to the backlight PWM (0-100). */
uint8_t bsp_lcd_br_get(void);

/**
 * Set the logical and hardware display rotation.
 *
 * esp_lvgl_port software rotation remains disabled, so the display driver
 * applies the requested rotation using the LCD hardware.
 */
esp_err_t bsp_lcd_set_rotation(bsp_lcd_rotation_t rotation);

/** Return the last successfully applied display rotation. */
bsp_lcd_rotation_t bsp_lcd_get_rotation(void);

/** Return the orientation derived from the applied display rotation. */
bsp_lcd_orientation_t bsp_lcd_get_orientation(void);

/** Return true when the applied display rotation uses a landscape layout. */
bool bsp_lcd_is_landscape(void);

/** Read the display's current LVGL logical resolution. */
esp_err_t bsp_lcd_get_logical_resolution(int32_t *width, int32_t *height);

#endif // OPEN_NEXTION_COMPONENTS_BSP_LCD_H
