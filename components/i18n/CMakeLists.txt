set(I18N_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
# IDF also reads component files in script mode to discover dependencies.
# Resource generation belongs only to the actual configure phase.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    file(GLOB I18N_RESOURCES "${CMAKE_CURRENT_LIST_DIR}/locales/*.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${I18N_RESOURCES} "${CMAKE_CURRENT_LIST_DIR}/tools/generate_i18n.py")
    idf_build_get_property(i18n_python PYTHON)
    execute_process(
        COMMAND "${i18n_python}" "${CMAKE_CURRENT_LIST_DIR}/tools/generate_i18n.py"
                --output "${I18N_GENERATED}"
        RESULT_VARIABLE I18N_RESULT)
    if(NOT I18N_RESULT EQUAL 0)
        message(FATAL_ERROR "i18n resource validation failed")
    endif()
endif()
idf_component_register(SRCS "i18n.c" "i18n_format.c" "i18n_compose.c"
    INCLUDE_DIRS "include" "${I18N_GENERATED}")
