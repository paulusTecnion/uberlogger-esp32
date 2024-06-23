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
        spiffs_create_partition_image(www ${WEB_SRC_DIR}/www FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "${WEB_SRC_DIR}/ doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
    endif()
endif()