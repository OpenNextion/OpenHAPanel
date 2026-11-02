idf_component_register(SRCS "screen_theme.c" "screen_theme_palette.c"
        INCLUDE_DIRS "include"
        REQUIRES esp_event
        PRIV_REQUIRES freertos ui_preferences)
