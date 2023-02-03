#ifndef _SPI_CTRL_H
#define _SPI_CTRL_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "common.h"
#include "settings.h"

esp_err_t spi_ctrl_init(uint8_t spicontroller, uint8_t gpio_data_ready_point);
esp_err_t spi_ctrl_enable();
esp_err_t spi_ctrl_disable();
esp_err_t spi_ctrl_getError();
uint8_t * spi_ctrl_getRxData();
esp_err_t

#endif