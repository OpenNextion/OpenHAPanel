idf_component_register(SRCS "screen_rotation.c"
        INCLUDE_DIRS "include"
        REQUIRES bsp_lcd
        PRIV_REQUIRES screen ui_preferences nvs_flash)
