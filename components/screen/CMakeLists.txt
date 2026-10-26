idf_component_register(SRCS "screen.c" "screen_api.c" "screen_validate.c"
        INCLUDE_DIRS "include"
        REQUIRES bsp_lcd esp_event json utils webserver
)
