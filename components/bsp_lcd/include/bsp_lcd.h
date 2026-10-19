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
#define LCD_PCLK_HZ             (80 * 1000 * 1000)
#define LCD_FLUSH_LINES         80

#define LVGL_BUF_LINES          80


esp_err_t bsp_lcd_init(void);
esp_err_t bsp_lcd_br_set(uint8_t br);
/** Last brightness successfully written to the backlight PWM (0-100). */
uint8_t bsp_lcd_br_get(void);
