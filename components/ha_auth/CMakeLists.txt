idf_component_register(
    SRCS "ha_auth.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_http_client esp_timer esp_hw_support json ha_ws utils freertos
)
