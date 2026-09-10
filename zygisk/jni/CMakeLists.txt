cmake_minimum_required(VERSION 3.22.1)
project(pogo_root_automation_zygisk LANGUAGES CXX)

# Default to the vendored Zygisk API header (third_party/zygisk.hpp). Override
# with -DZYGISK_API_DIR=<dir> to point at a different copy.
if(NOT DEFINED ZYGISK_API_DIR)
    set(ZYGISK_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
endif()
if(NOT EXISTS "${ZYGISK_API_DIR}/zygisk.hpp")
    message(FATAL_ERROR "zygisk.hpp not found in ${ZYGISK_API_DIR}; set -DZYGISK_API_DIR")
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
