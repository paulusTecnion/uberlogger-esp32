/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#ifndef _SPI_CTRL_H
#define _SPI_CTRL_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "common.h"
#include "esp_err.h"
#include "ul_protocol.h"  // shared SPI protocol: stm32cmd_t, spi_cmd_t, stm32resp_t

typedef enum uint8_t {
    RXDATA_STATE_NODATA = 0,
    RXDATA_STATE_DATA_READY,
    RXDATA_STATE_DATA_OVERRUN
} rxdata_state_t;

// stm32cmd_t (command enum), spi_cmd_t (8-byte command struct) and stm32resp_t
// (STM32_RESP_OK/NOK) now live in the shared single source of truth ul_protocol.h.

esp_err_t spi_ctrl_init(uint8_t spicontroller, uint8_t gpio_data_ready_point);
esp_err_t spi_ctrl_cmd(stm32cmd_t cmd, spi_cmd_t* cmd_data, size_t rx_data_length);
esp_err_t spi_ctrl_enable();
esp_err_t spi_ctrl_disable();
esp_err_t spi_ctrl_getError();
uint8_t * spi_ctrl_getRxData();
esp_err_t spi_ctrl_datardy_int(uint8_t value);
void spi_ctrl_reset_rx_state();
void spi_ctrl_loop();

void spi_ctrl_print_rx_buffer();
esp_err_t spi_ctrl_queue_msg(uint8_t * txData, size_t length);
/// @brief Check for the last queued message
/// @return Returns ESP_OK when the message is received
esp_err_t spi_ctrl_receive_data();
rxdata_state_t spi_ctrl_rxstate();

#endif