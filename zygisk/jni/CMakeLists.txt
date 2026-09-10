cmake_minimum_required(VERSION 3.22.1)
project(pogo_root_automation_zygisk LANGUAGES CXX)

if(NOT DEFINED ZYGISK_API_DIR)
    message(FATAL_ERROR "Set -DZYGISK_API_DIR to the directory containing zygisk.hpp")
endif()

add_library(pogo_root_automation SHARED main.cpp)

target_include_directories(
    pogo_root_automation
    PRIVATE
    ${ZYGISK_API_DIR}
)

target_compile_features(pogo_root_automation PRIVATE cxx_std_17)
target_compile_options(pogo_root_automation PRIVATE -Wall -Wextra -Werror)
target_link_libraries(pogo_root_automation log dl)
