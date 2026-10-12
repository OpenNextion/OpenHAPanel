#include "esp_err.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_types.h"

typedef struct {
    int cmd;
    const void *data;
    size_t data_bytes;
    unsigned int delay_ms;
} st7796u_lcd_init_cmd_t;

typedef struct {
    const st7796u_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} st7796u_vendor_config_t;

esp_err_t esp_lcd_new_panel_st7796u(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);
