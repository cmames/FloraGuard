cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{PIOENV} AND "$ENV{PIOENV}" STREQUAL "calibration")
    idf_component_register()
else()
    idf_component_register(
        SRCS "wifi_manager.c"
        INCLUDE_DIRS "include"
        REQUIRES esp_wifi esp_event mdns nvs_flash log_manager
    )
endif()