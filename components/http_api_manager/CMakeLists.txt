cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{PIOENV} AND "$ENV{PIOENV}" STREQUAL "calibration")
    idf_component_register()
else()
    idf_component_register(
        SRCS "http_api_manager.c"
        INCLUDE_DIRS "include"
        REQUIRES esp_http_server soil_moisture bme280_manager actuator_manager log_manager
    )
endif()