idf_component_register(SRCS 
                            "console.c"
                            "file_server.c"
                            "fileman.c"
                            "firmwareESP32.c"
                            "firmwareSTM32.c"
                            "firmware-www.c"
                            "hmi.c"         
                            "iirfilter.c"            
                            "logger.c"                    
                            "main.c"
                            "rest_server.c"
                            "settings.c"
                            "spi_control.c"
                            "sysinfo.c"
                            "tempsensor.c"
                            "wifi.c" 
                            
                    INCLUDE_DIRS "" 
                    EMBED_FILES 
                    # "favicon.ico" 
                    "upload_script.html")

if(CONFIG_EXAMPLE_WEB_DEPLOY_SF)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front")
    if(EXISTS ${WEB_SRC_DIR}/www)
        # Stage front/www into the build dir with the .html/.css/.js files gzipped
        # (the firmware serves name.gz for name), then build the SPIFFS image from it.
        idf_build_get_property(python PYTHON)
        set(WWW_STAGE_DIR "${CMAKE_BINARY_DIR}/www_stage")
        add_custom_target(www_stage ALL
            COMMAND ${python} ${WEB_SRC_DIR}/build_www.py ${WEB_SRC_DIR}/www ${WWW_STAGE_DIR}
            COMMENT "Staging and compressing the web UI"
            VERBATIM)
        spiffs_create_partition_image(www ${WWW_STAGE_DIR} FLASH_IN_PROJECT DEPENDS www_stage)
    else()
        message(FATAL_ERROR "${WEB_SRC_DIR}/ doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
    endif()
endif()