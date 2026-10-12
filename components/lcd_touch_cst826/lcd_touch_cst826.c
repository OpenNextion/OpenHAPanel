#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_types.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

#define POINT_NUM_MAX       2

#define DATA_START_REG      0x02
#define CHIP_ID_REG         0xAA

static esp_err_t esp_lcd_touch_cst826_read_data(esp_lcd_touch_handle_t tp);
static bool esp_lcd_touch_cst826_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t esp_lcd_touch_cst826_del(esp_lcd_touch_handle_t tp);

static esp_err_t touch_cst826_reset(esp_lcd_touch_handle_t tp);
static esp_err_t touch_cst826_read_id(esp_lcd_touch_handle_t tp);
static esp_err_t touch_cst826_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);

static const char *TAG = "cst826";

esp_err_t esp_lcd_touch_new_i2c_cst826(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp)
{
    ESP_RETURN_ON_FALSE(io != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller io handle can not be NULL");
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer to the touch controller configuration can not be NULL");
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer to the touch controller handle can not be NULL");

    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t cst826 = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(cst826, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    cst826->io = io;
    cst826->read_data = esp_lcd_touch_cst826_read_data;
    cst826->get_xy = esp_lcd_touch_cst826_get_xy;
    cst826->del = esp_lcd_touch_cst826_del;
    cst826->data.lock.owner = portMUX_FREE_VAL;
    memcpy(&cst826->config, config, sizeof(esp_lcd_touch_config_t));

    if (cst826->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t io_conf = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = cst826->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
            .pin_bit_mask = BIT64(cst826->config.int_gpio_num),
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "GPIO intr config failed");

        if (cst826->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(cst826, cst826->config.interrupt_callback);
        }
    }

    if (cst826->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(cst826->config.rst_gpio_num),
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "GPIO reset config failed");
    }

    ESP_GOTO_ON_ERROR(touch_cst826_reset(cst826), err, TAG, "Reset failed");
    ESP_GOTO_ON_ERROR(touch_cst826_read_id(cst826), err, TAG, "Read ID failed");

    *tp = cst826;

    return ESP_OK;

err:
    if (cst826) {
        esp_lcd_touch_cst826_del(cst826);
    }
    ESP_LOGE(TAG, "Init failed");
    return ret;
}

static esp_err_t esp_lcd_touch_cst826_read_data(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can not be NULL");

    typedef struct {
        uint8_t num;
        uint8_t x_h : 4;
        uint8_t : 4;
        uint8_t x_l;
        uint8_t y_h : 4;
        uint8_t : 4;
        uint8_t y_l;
    } data_t;

    data_t point;
    ESP_RETURN_ON_ERROR(touch_cst826_i2c_read(tp, DATA_START_REG, (uint8_t *)&point, sizeof(data_t)), TAG, "I2C read failed");

    portENTER_CRITICAL(&tp->data.lock);
    point.num = (point.num > POINT_NUM_MAX) ? POINT_NUM_MAX : point.num;
    tp->data.points = point.num;
    for (int i = 0; i < point.num; i++) {
        tp->data.coords[i].x = point.x_h << 8 | point.x_l;
        tp->data.coords[i].y = point.y_h << 8 | point.y_l;
    }
    portEXIT_CRITICAL(&tp->data.lock);

    return ESP_OK;
}

static bool esp_lcd_touch_cst826_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    ESP_RETURN_ON_FALSE(tp != NULL, false, TAG, "Touch controller handle can not be NULL");
    ESP_RETURN_ON_FALSE(x != NULL, false, TAG, "Pointer to the x coordinates array can not be NULL");
    ESP_RETURN_ON_FALSE(y != NULL, false, TAG, "Pointer to the y coordinates array can not be NULL");
    ESP_RETURN_ON_FALSE(point_num != NULL, false, TAG, "Pointer to number of touch points can not be NULL");
    ESP_RETURN_ON_FALSE(max_point_num > 0, false, TAG, "Array size must be equal or larger than 1");

    portENTER_CRITICAL(&tp->data.lock);
    *point_num = (tp->data.points > max_point_num) ? max_point_num : tp->data.points;
    for (size_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;

        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }
    }
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t esp_lcd_touch_cst826_del(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can not be NULL");

    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }

    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }

    free(tp);

    return ESP_OK;
}

static esp_err_t touch_cst826_reset(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can not be NULL");

    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    return ESP_OK;
}

static esp_err_t touch_cst826_read_id(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can not be NULL");

    uint8_t id;
    ESP_RETURN_ON_ERROR(touch_cst826_i2c_read(tp, CHIP_ID_REG, &id, 1), TAG, "I2C read failed");
    ESP_LOGI(TAG, "IC id: %d", id);

    return ESP_OK;
}

static esp_err_t touch_cst826_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can not be NULL");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer to the data array can not be NULL");

    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}
