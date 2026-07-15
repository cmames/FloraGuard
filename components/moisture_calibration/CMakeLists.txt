idf_component_register(
    SRCS "calibration.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc log freertos log_manager
)