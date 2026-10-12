idf_component_register(
    SRCS "firmware_update.c"
         "firmware_auto_check.c"
         "firmware_boot_check.c"
         "firmware_download.c"
         "firmware_manifest.c"
         "firmware_manifest_download.c"
         "firmware_semver.c"
         "firmware_storage.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_event
    PRIV_REQUIRES app_update esp_app_format esp_http_client esp_timer freertos json lwip mbedtls network_manage sys_time utils
)
