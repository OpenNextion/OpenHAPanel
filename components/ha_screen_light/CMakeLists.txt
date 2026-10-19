idf_component_register(SRCS "ha_screen_light.c"
    INCLUDE_DIRS "include"
    REQUIRES ha_ws ha_auth bsp_lcd web webserver screen_night_mode
    PRIV_REQUIRES esp_http_client esp_hw_support freertos json utils)
