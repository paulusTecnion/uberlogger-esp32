idf_component_register(SRCS "fileman.c"
                    INCLUDE_DIRS .
                    REQUIRES fatfs vfs sdmmc)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")