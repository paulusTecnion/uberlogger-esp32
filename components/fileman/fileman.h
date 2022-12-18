#pragma once

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"


esp_err_t fileman_open_file(void);
esp_err_t fileman_close_file(void);
esp_err_t fileman_search_last_sequence_file(void);
int fileman_write(const void * data, size_t len);
int fileman_csv_write(const void * data, size_t len);