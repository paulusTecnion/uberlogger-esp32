# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
add_custom_target(
    RunMyScript ALL
    COMMAND python ${CMAKE_SOURCE_DIR}/update_version.py
)

cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(uberlogger-esp32)
