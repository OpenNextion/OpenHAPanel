idf_component_register(SRCS "key.c"
        INCLUDE_DIRS "include"
        REQUIRES driver esp_event
        PRIV_REQUIRES espressif__button esp_timer)
