#include "esp_err.h"

#define PCF8574_I2C_SPEED_HZ        (100 * 1000)

#define PCF8574_I2C_ADDR            0x38

#define PCF8574_I2C_RW_TIMEOUT_MS   500

typedef enum {
    EXIO_PCF8574_PIN_I2S_CTRL = 1,
    EXIO_PCF8574_PIN_RTC_INT,
    EXIO_PCF8574_PIN_CHG_N,
    EXIO_PCF8574_PIN_CAM_PWDN,
    EXIO_PCF8574_PIN_TP_INT,
    EXIO_PCF8574_PIN_LCD_RST,
    EXIO_PCF8574_PIN_SDCS,
} exio_pcf8574_pin;

typedef enum {
    EXIO_PCF8574_LEVEL_L = 0,
    EXIO_PCF8574_LEVEL_H,
} exio_pcf8574_level;

esp_err_t exio_pcf8574_pin_read(exio_pcf8574_pin pin, exio_pcf8574_level *level);

esp_err_t exio_pcf8574_pin_write(exio_pcf8574_pin pin, exio_pcf8574_level level);

esp_err_t exio_pcf8574_init(void);
