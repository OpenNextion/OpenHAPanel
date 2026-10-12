idf_component_register(SRCS "cjson_helpers.c" "littlefs.c" "check_ram.c" "uuid4.c" "nvs_storage.c"
        INCLUDE_DIRS "include"
        REQUIRES json nvs_flash
        PRIV_REQUIRES joltwallet__littlefs littlefs esp_partition esp_timer
)
