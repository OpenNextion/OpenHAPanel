idf_component_register(
    SRCS "ha_entity_store.c"
    INCLUDE_DIRS "include"
    REQUIRES ha_ws json freertos esp_timer
)
