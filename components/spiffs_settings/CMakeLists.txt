idf_component_register(SRCS "spiffs_settings.c"
                    INCLUDE_DIRS .
                    REQUIRES vfs spiffs)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")