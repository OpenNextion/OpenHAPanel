idf_component_register(SRCS "webserver.c" "webserver_message_parse.c" "webserver_auth.c" "webserver_api.c" "webserver_client_node.c" "webserver_ha_auth.c" "webserver_login_session.c" "webserver_provisioning_handoff.c" "webserver_screen.c" "webserver_firmware_update.c" "webserver_screen_rotation.c" "webserver_screen_theme.c"
        REQUIRES esp_http_server json utils freertos
        PRIV_REQUIRES i18n esp_wifi esp_eth mbedtls freertos esp_timer web sys_time network_manage screen screen_state screen_rotation screen_theme ha_auth mdns login_session bsp_lcd lan_mdns screen_night_mode

        INCLUDE_DIRS "include"
        EMBED_FILES "index.html.gz")
