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

# Copy OTA binaries to release/ after every successful build.
# Cross-platform via `cmake -E`; copy_if_different keeps timestamps stable so
# incremental rebuilds don't re-touch the output files.
add_custom_target(release_binaries ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/release
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
        ${CMAKE_SOURCE_DIR}/release/ota_main.bin
    COMMENT "Copying ota_main.bin to release/"
    VERBATIM
)
add_dependencies(release_binaries app)

if(TARGET spiffs_www_bin)
    add_custom_target(release_filesystem ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/www.bin
            ${CMAKE_SOURCE_DIR}/release/ota_filesystem.bin
        COMMENT "Copying ota_filesystem.bin to release/"
        VERBATIM
    )
    add_dependencies(release_filesystem spiffs_www_bin release_binaries)
endif()
