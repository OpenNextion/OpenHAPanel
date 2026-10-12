idf_component_register(
    SRCS "login_session.c"
    INCLUDE_DIRS "include"
    REQUIRES mbedtls utils freertos esp_hw_support
)
