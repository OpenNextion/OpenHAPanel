idf_component_register(
    SRCS "screen_night_mode.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event freertos json utils webserver web bsp_lcd sys_time
)