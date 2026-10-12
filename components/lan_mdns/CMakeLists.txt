idf_component_register(SRCS "mdns_api.c" "mdns_init.c"
        INCLUDE_DIRS "include"
        REQUIRES utils mdns wireless_controller
        PRIV_REQUIRES esp_netif lwip mbedtls esp_wifi nvs_flash json network_manage webserver web
        
)
