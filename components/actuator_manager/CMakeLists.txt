cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "actuator_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver log_manager
)