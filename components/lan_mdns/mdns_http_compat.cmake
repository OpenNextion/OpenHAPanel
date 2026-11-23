# Espressif mDNS 1.11.3 emits A/AAAA glue with HTTP service replies.
# Use the project copy of mdns_send.c so HTTP discovery does not bundle host
# addresses, while direct hostname queries and other services stay intact.
idf_component_get_property(ha_mdns_lib espressif__mdns COMPONENT_LIB)
get_target_property(ha_mdns_sources ${ha_mdns_lib} SOURCES)
set(ha_mdns_send_source "")
foreach(ha_mdns_source IN LISTS ha_mdns_sources)
    if(ha_mdns_source MATCHES "(^|/)mdns_send\\.c$")
        set(ha_mdns_send_source "${ha_mdns_source}")
        break()
    endif()
endforeach()
if(NOT ha_mdns_send_source)
    message(FATAL_ERROR "Unable to locate Espressif mDNS sender for HTTP compatibility fix")
endif()
list(REMOVE_ITEM ha_mdns_sources "${ha_mdns_send_source}")
set_property(TARGET ${ha_mdns_lib} PROPERTY SOURCES "${ha_mdns_sources}")
target_sources(${ha_mdns_lib} PRIVATE
    "${CMAKE_SOURCE_DIR}/components/lan_mdns/mdns_send_http_compat.c")
