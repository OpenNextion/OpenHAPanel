idf_component_register(SRCS "sys_time.c" "sys_time_sync.c" "sys_time_api.c"
        INCLUDE_DIRS "include"
        REQUIRES esp_event
        PRIV_REQUIRES esp_netif lwip mbedtls esp_wifi nvs_flash
        esp_http_client json utils network_manage web webserver)
