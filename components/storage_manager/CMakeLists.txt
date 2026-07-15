cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "storage_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES spiffs log_manager
)