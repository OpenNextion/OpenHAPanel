idf_component_register(SRCS "webserver_config.c" "webserver_config_api.c" "webserver_config_backup.c" "webserver_config_forget_password.c"
        INCLUDE_DIRS "include"
        REQUIRES utils screen_state
        PRIV_REQUIRES mbedtls vfs webserver esp_wifi sys_time network_manage key ha_auth login_session ui_preferences firmware_update
)
