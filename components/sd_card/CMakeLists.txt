idf_component_register(SRCS "esp_sd_card.c"
                    INCLUDE_DIRS .
                    REQUIRES fatfs vfs sdmmc)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")