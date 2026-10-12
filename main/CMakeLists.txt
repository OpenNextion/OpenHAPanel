# Keep UI assets (including OTA state artwork and status icons) under assets/ so a normal
# CMake reconfigure discovers them.
file(GLOB_RECURSE HA_UI_ASSET_SRCS
        RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/assets/*.c"
        "${CMAKE_CURRENT_LIST_DIR}/assets/images/*.c"
)

# UI widgets, including transient components such as toast.c, are discovered
# when CMake configures the main component.
file(GLOB_RECURSE HA_UI_WIDGET_SRCS
        RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/screen/components/*.c"
)

file(GLOB_RECURSE HA_ENTITY_HANDLER_SRCS
        RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/entity_handlers/*.c"
)

idf_component_register(SRCS "main.c" "app.c"
                            "screen/screen_ui.c"
                            "screen/screen_config.c"
                            "screen/entity_display.c"
                        ${HA_UI_ASSET_SRCS}
                        ${HA_UI_WIDGET_SRCS}
                        ${HA_ENTITY_HANDLER_SRCS}
                    INCLUDE_DIRS "." "screen" "screen/components")
