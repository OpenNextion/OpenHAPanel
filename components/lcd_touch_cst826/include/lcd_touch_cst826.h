#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_types.h"

#define LCD_TOUCH_CST826_I2C_ADDR       0x15
#define LCD_TOUCH_CST826_I2C_SPEED_HZ   (400 * 1000)

esp_err_t esp_lcd_touch_new_i2c_cst826(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp);
