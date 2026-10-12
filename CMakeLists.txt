# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if (NOT DEFINED FIRMWARE_VERSION)
    set(FIRMWARE_VERSION "0.0.1")
endif ()

# stable/beta
if (NOT DEFINED FIRMWARE_TYPE)
    set(FIRMWARE_TYPE "stable")
endif ()

# Hardware model compiled into the firmware and OTA identity. Keep the
# release workflow explicit even though there is currently only one model.
if (NOT DEFINED DEVICE_MODEL)
    set(DEVICE_MODEL "ONX3248G035")
endif ()

set(SUPPORTED_DEVICE_MODELS ONX3248G035)
if (NOT DEVICE_MODEL IN_LIST SUPPORTED_DEVICE_MODELS)
    message(FATAL_ERROR "Unsupported DEVICE_MODEL '${DEVICE_MODEL}'")
endif ()
string(TOLOWER "${DEVICE_MODEL}" DEVICE_MODEL_SLUG)
string(REPLACE "_" "-" DEVICE_MODEL_SLUG "${DEVICE_MODEL_SLUG}")

set(VALID_FIRMWARE_TYPES stable beta)
if (NOT FIRMWARE_TYPE IN_LIST VALID_FIRMWARE_TYPES)
    message(FATAL_ERROR "FIRMWARE_TYPE must be 'stable' or 'beta', got '${FIRMWARE_TYPE}'")
endif ()

if (NOT DEFINED OTA_DEFAULT_CHANNEL)
    set(OTA_DEFAULT_CHANNEL "stable")
endif ()

if (NOT OTA_DEFAULT_CHANNEL IN_LIST VALID_FIRMWARE_TYPES)
    message(FATAL_ERROR "OTA_DEFAULT_CHANNEL must be 'stable' or 'beta', got '${OTA_DEFAULT_CHANNEL}'")
endif ()

# Automatic update checks run after every successful Wi-Fi connection and once
# per local calendar day. The daily execution minute is randomized to spread
# server load; a missed daily window is not replayed later in the day.
if (NOT DEFINED OTA_AUTO_CHECK_DEFAULT)
    set(OTA_AUTO_CHECK_DEFAULT 1)
endif ()

if (NOT OTA_AUTO_CHECK_DEFAULT MATCHES "^[01]$")
    message(FATAL_ERROR "OTA_AUTO_CHECK_DEFAULT must be 0 or 1")
endif ()

# Run one Manifest-only automatic check after every new Wi-Fi connection gets
# an IP address and local time is ready. This trigger coexists with the daily
# randomized download schedule.
if (NOT DEFINED OTA_AUTO_CHECK_ON_WIFI_CONNECT)
    set(OTA_AUTO_CHECK_ON_WIFI_CONNECT 1)
endif ()

if (NOT OTA_AUTO_CHECK_ON_WIFI_CONNECT MATCHES "^[01]$")
    message(FATAL_ERROR "OTA_AUTO_CHECK_ON_WIFI_CONNECT must be 0 or 1")
endif ()

if (NOT DEFINED OTA_AUTO_CHECK_START_HOUR)
    set(OTA_AUTO_CHECK_START_HOUR 0)
endif ()

if (NOT OTA_AUTO_CHECK_START_HOUR MATCHES "^[0-9]+$" OR
    OTA_AUTO_CHECK_START_HOUR GREATER 23)
    message(FATAL_ERROR "OTA_AUTO_CHECK_START_HOUR must be in range 0..23")
endif ()

if (NOT DEFINED OTA_AUTO_CHECK_WINDOW_MINUTES)
    set(OTA_AUTO_CHECK_WINDOW_MINUTES 60)
endif ()

math(EXPR OTA_AUTO_CHECK_MAX_WINDOW "1440 - ${OTA_AUTO_CHECK_START_HOUR} * 60")
if (NOT OTA_AUTO_CHECK_WINDOW_MINUTES MATCHES "^[0-9]+$" OR
    OTA_AUTO_CHECK_WINDOW_MINUTES LESS 1 OR
    OTA_AUTO_CHECK_WINDOW_MINUTES GREATER OTA_AUTO_CHECK_MAX_WINDOW)
    message(FATAL_ERROR
        "OTA_AUTO_CHECK_WINDOW_MINUTES must fit between the start hour and midnight")
endif ()

if (NOT DEFINED SDKCONFIG)
    set(SDKCONFIG "sdkconfig")
endif ()

if (NOT DEFINED OTA_INFO_URL)
    set(OTA_INFO_URL "https://github.com/OpenNextion/OpenHAPanel/releases/latest/download/open-ha-panel-${DEVICE_MODEL_SLUG}.manifest.json")
endif ()

# GitHub owner/repository used to validate firmware and release URLs in a
# downloaded Manifest. Override it together with OTA_INFO_URL for staging or
# after migrating the production repository.
if (NOT DEFINED OTA_RELEASE_REPOSITORY)
    set(OTA_RELEASE_REPOSITORY "OpenNextion/OpenHAPanel")
endif ()

if (NOT OTA_RELEASE_REPOSITORY MATCHES "^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
    message(FATAL_ERROR
        "OTA_RELEASE_REPOSITORY must use the GitHub owner/repository form, got '${OTA_RELEASE_REPOSITORY}'")
endif ()

# Development-only escape hatch for serving OTA artifacts from a trusted LAN
# HTTP server. Production builds must leave this disabled.
if (NOT DEFINED OTA_ALLOW_INSECURE_HTTP)
    set(OTA_ALLOW_INSECURE_HTTP 0)
endif ()

if (NOT OTA_ALLOW_INSECURE_HTTP MATCHES "^[01]$")
    message(FATAL_ERROR "OTA_ALLOW_INSECURE_HTTP must be 0 or 1")
endif ()

if (NOT OTA_INFO_URL MATCHES "^https://" AND NOT OTA_INFO_URL MATCHES "^http://")
    message(FATAL_ERROR "OTA_INFO_URL must use HTTP or HTTPS")
endif ()

if (NOT OTA_ALLOW_INSECURE_HTTP AND NOT OTA_INFO_URL MATCHES "^https://")
    message(FATAL_ERROR "OTA_INFO_URL must use HTTPS unless OTA_ALLOW_INSECURE_HTTP=1")
endif ()

if (OTA_ALLOW_INSECURE_HTTP)
    message(WARNING "Insecure local OTA test mode is enabled; do not publish this build")
endif ()

# Git tags use the v1.2.3 form, while the application descriptor and OTA
# manifest use plain SemVer (1.2.3).
set(PROJECT_VER "${FIRMWARE_VERSION}")

add_compile_definitions("CONFIG_FIRMWARE_VERSION_STRING=\"${FIRMWARE_VERSION}\"")
add_compile_definitions("CONFIG_FIRMWARE_TYPE_STRING=\"${FIRMWARE_TYPE}\"")
add_compile_definitions("CONFIG_DEVICE_MODEL_STRING=\"${DEVICE_MODEL}\"")
add_compile_definitions("CONFIG_DEVICE_MODEL_SLUG=\"${DEVICE_MODEL_SLUG}\"")
add_compile_definitions("OTA_INFO_URL=\"${OTA_INFO_URL}\"")
add_compile_definitions("OTA_RELEASE_REPOSITORY=\"${OTA_RELEASE_REPOSITORY}\"")
add_compile_definitions("OTA_DEFAULT_CHANNEL_STRING=\"${OTA_DEFAULT_CHANNEL}\"")
add_compile_definitions("OTA_AUTO_CHECK_DEFAULT=${OTA_AUTO_CHECK_DEFAULT}")
add_compile_definitions("OTA_AUTO_CHECK_ON_WIFI_CONNECT=${OTA_AUTO_CHECK_ON_WIFI_CONNECT}")
add_compile_definitions("OTA_AUTO_CHECK_START_HOUR=${OTA_AUTO_CHECK_START_HOUR}")
add_compile_definitions("OTA_AUTO_CHECK_WINDOW_MINUTES=${OTA_AUTO_CHECK_WINDOW_MINUTES}")
add_compile_definitions("OTA_ALLOW_INSECURE_HTTP=${OTA_ALLOW_INSECURE_HTTP}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(open-ha-panel)

configure_file(
    "${CMAKE_SOURCE_DIR}/tools/open_ha_panel_build_metadata.json.in"
    "${CMAKE_BINARY_DIR}/open_ha_panel_build_metadata.json"
    @ONLY
)
