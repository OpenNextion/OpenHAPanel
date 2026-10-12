idf_component_register(
    SRCS "ha_ws.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_websocket_client json esp_event freertos
)
