# Keep UI assets (including OTA state artwork and status icons) under assets/ so a normal
# CMake reconfigure discovers them.
file(GLOB_RECURSE HA_UI_ASSET_SRCS
        RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/assets/*.c"
        "${CMAKE_CURRENT_LIST_DIR}/assets/images/*.c"
)

# UI widgets, including screen_indicator.c and transient components such as
# toast.c, are discovered when CMake configures the main component.
file(GLOB_RECURSE HA_UI_WIDGET_SRCS
        RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/screen/components/*.c"
)

# Entity handlers also include their small domain-specific stores, such as the
# Home Assistant light favorite-color options cache.
file(GLOB_RECURSE HA_ENTITY_HANDLER_SRCS
        RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/entity_handlers/*.c"
)

file(GLOB_RECURSE HA_COMMON_SRCS
        RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/common/*.c"
)

idf_component_register(SRCS "main.c" "app.c" "font_render_worker.c" "ha_ui_font.c"
                            "screen/screen_ui.c"
                            "screen/screen_config.c"
                            "screen/entity_display.c"
                        ${HA_UI_ASSET_SRCS}
                        ${HA_UI_WIDGET_SRCS}
                        ${HA_ENTITY_HANDLER_SRCS}
                        ${HA_COMMON_SRCS}
                    INCLUDE_DIRS "." "screen" "screen/components")

# Keep the font in the OTA application so factory and OTA images are both
# self-contained. Compress it to leave headroom in the 6 MiB app partition.
set(HA_FONT_SOURCE "${CMAKE_CURRENT_LIST_DIR}/assets/fonts/NotoSansKR-Regular.ttf")
set(HA_FONT_PACKED "${CMAKE_CURRENT_BINARY_DIR}/ha_ui_font.zlib")
set(HA_FONT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/ha_ui_font_data.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${HA_FONT_SOURCE}" "${CMAKE_SOURCE_DIR}/tools/pack_font.py")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_SOURCE_DIR}/tools/pack_font.py"
        "${HA_FONT_SOURCE}" "${HA_FONT_PACKED}" "${HA_FONT_HEADER}"
    RESULT_VARIABLE HA_FONT_PACK_RESULT
)
if(NOT HA_FONT_PACK_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to pack UI TTF")
endif()
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_add_binary_data(${COMPONENT_LIB} "${HA_FONT_PACKED}" BINARY
    RENAME_TO ha_ui_font_zlib)
