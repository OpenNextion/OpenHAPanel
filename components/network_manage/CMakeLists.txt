idf_component_register(SRCS "network.c" "network_wifi_ap.c" "network_wifi_sta.c" "network_captive_portal.c" "network_api.c" "network_file.c" "network_ping.c"
        INCLUDE_DIRS "include"
        REQUIRES utils
        PRIV_REQUIRES nvs_flash esp_netif esp_wifi lwip esp_http_server json webserver web lan_mdns esp_driver_gpio mbedtls
)

