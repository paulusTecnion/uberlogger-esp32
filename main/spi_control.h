#ifndef _SPI_CTRL_H
#define _SPI_CTRL_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "common.h"
#include "esp_err.h"

typedef enum uint8_t {
    RXDATA_STATE_NODATA = 0,
    RXDATA_STATE_DATA_READY,
    RXDATA_STATE_DATA_OVERRUN
} rxdata_state_t;

typedef enum stm32cmd {
    STM32_CMD_NOP=0,
    STM32_CMD_SETTINGS_MODE,
    STM32_CMD_SETTINGS_SYNC,
    STM32_CMD_MEASURE_MODE,
    STM32_CMD_SET_RESOLUTION,
    STM32_CMD_SET_SAMPLE_RATE,
    STM32_CMD_SET_ADC_CHANNELS_ENABLED,
    STM32_CMD_SINGLE_SHOT_MEASUREMENT,
} stm32cmd_t;

typedef enum stm32resp {
    STM32_RESP_OK = 1,
    STM32_RESP_NOK
 } stm32resp_t;

 // Struct for sending SPI commands to the STM32
typedef struct {
    uint8_t command;
    uint8_t data;
    // this dummy byte is necessary for the FIFO buffer of the 
    // SPI controller on the STM32 to fill up. Else if wait forever
    // to complete the transaction.
    uint16_t dummy;    
} spi_cmd_t;

esp_err_t spi_ctrl_init(uint8_t spicontroller, uint8_t gpio_data_ready_point);
esp_err_t spi_ctrl_cmd(stm32cmd_t cmd, uint8_t data);
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