#pragma once

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"

#define SPIFFS_LABEL "data"

typedef enum  {
    SPIFFS_READ = 0,
    SPIFFS_WRITE,
    SPIFFS_CHECK_FILE_EXISTS
} spiffs_mode_t;

esp_err_t spiffs_init(const char * filename);
esp_err_t spiffs_read(char* data, size_t length);
esp_err_t spiffs_write(const char* data, size_t length);
