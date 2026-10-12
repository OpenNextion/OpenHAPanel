#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "exio_pcf8574.h"

static const char *TAG = "pcf8574";

static volatile uint8_t pcf8574_io_state = 0xFF;

static i2c_master_dev_handle_t pcf8574_i2c_handle = NULL;

esp_err_t exio_pcf8574_pin_read(exio_pcf8574_pin pin, exio_pcf8574_level *level)
{
    esp_err_t ret;
    uint8_t bit_mask = (0x01 << (pin - 1));
    uint8_t read_io_state = 0x00;

    ret = i2c_master_receive(pcf8574_i2c_handle, &read_io_state, 1, PCF8574_I2C_RW_TIMEOUT_MS);
    if (ret == ESP_OK) {
        if (read_io_state & bit_mask) {
            *level = EXIO_PCF8574_LEVEL_H;
        } else {
            *level = EXIO_PCF8574_LEVEL_L;
        }
    }

    return ret;
}

esp_err_t exio_pcf8574_pin_write(exio_pcf8574_pin pin, exio_pcf8574_level level)
{
    esp_err_t ret;
    uint8_t bit_mask = 0x01;
    uint8_t io_state = 0x00;

    if (level == EXIO_PCF8574_LEVEL_H) {
        pcf8574_io_state |= (bit_mask << (pin - 1));
    } else if (level == EXIO_PCF8574_LEVEL_L) {
        pcf8574_io_state &= (~(bit_mask << (pin - 1)));
    } else {
        ESP_LOGW(TAG, "Invalid write pin: %d", level);
        return ESP_FAIL;
    }

    io_state = pcf8574_io_state;
    ret = i2c_master_transmit(pcf8574_i2c_handle, &io_state, 1, PCF8574_I2C_RW_TIMEOUT_MS);
    return ret;
}

esp_err_t exio_pcf8574_init(void)
{
    i2c_master_bus_handle_t i2c_bus;
    ESP_RETURN_ON_ERROR(i2c_master_get_bus_handle(I2C_NUM_0, &i2c_bus), TAG, "Get I2C bus handle failed");

    i2c_device_config_t dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = PCF8574_I2C_SPEED_HZ,
        .device_address = PCF8574_I2C_ADDR,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus, &dev_conf, &pcf8574_i2c_handle), TAG, "Add device failed");

    return ESP_OK;
}
