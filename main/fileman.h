#pragma once

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "logger.h"



esp_err_t fileman_open_file();
esp_err_t fileman_close_file(void);
uint32_t fileman_search_last_sequence_file(const char * prefix);
esp_err_t fileman_set_prefix(const char * prefix, time_t timestamp, uint8_t continueNumbering);
uint8_t fileman_check_current_file_size(size_t sizeInBytes);
void fileman_reset_subnum(void);
int fileman_write(const void * data, size_t len);
int fileman_csv_write(const int32_t *dataAdc, const uint8_t *dataGpio,  const s_date_time_t *date_time_ptr, size_t datarows);
int fileman_csv_write_header(void);
esp_err_t fileman_raw_write_header(void);
esp_err_t fileman_csv_write_spi_msg(sdcard_data_t *sdcard_data, const int32_t * adcData);

