#pragma once

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"


esp_err_t fileman_open_file();
esp_err_t fileman_close_file(void);
uint32_t fileman_search_last_sequence_file(void);
esp_err_t fileman_set_prefix(const char * prefix);
uint8_t fileman_check_current_file_size(size_t sizeInBytes);
void fileman_reset_subnum(void);
int fileman_write(const void * data, size_t len);
int fileman_csv_write(const int32_t * dataAdc,  size_t lenAdc, const uint8_t* dataGpio, size_t lenGpio, const uint8_t* dataTime, size_t lenTime, size_t datarows);
int fileman_csv_write_header(void);