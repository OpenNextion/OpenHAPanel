idf_component_register(SRCS "bsp_lcd.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "driver" "esp_lcd" "exio_pcf8574" "lcd_st7796u" "lcd_touch_cst826" "esp_lvgl_port")
