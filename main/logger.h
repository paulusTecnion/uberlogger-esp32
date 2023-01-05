#ifndef _LOGGER_H
#define _LOGGER_H
#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "settings.h"


enum LogTaskStates
{
    LOGTASK_INIT = 0,
    LOGTASK_IDLE,
    LOGTASK_LOGGING,
    LOGTASK_SETTINGS,
    LOGTASK_NUM_STATES
};


enum LoggingStates{
    LOGGING_START = 0,
    LOGGING_RX0_WAIT,
    LOGGING_RX1_WAIT,
    LOGGING_STOP,
    LOGGING_NUM_STATES
};

typedef enum stm32cmd {
    STM32_CMD_NOP=0,
    STM32_CMD_SETTINGS_MODE,
    STM32_CMD_MEASURE_MODE,
    STM32_CMD_SET_RESOLUTION,
    STM32_CMD_SET_ADC_CHANNELS_ENABLED,
    STM32_CMD_SET_SAMPLE_RATE
} stm32cmd_t;

typedef enum stm32resp {
    STM32_RESP_OK = 1,
    STM32_RESP_NOK
 } stm32resp_t;

typedef uint8_t LoggerState_t;
typedef uint8_t LoggingState_t;


/**
 * @brief Enable or disable the interrupt for the data ready pin.
 *
 *
 * @note This function is non-blocking
 *
 * @param value 0 for disable, 1 for enable
 * @return
 *         - RET_NOK  if interrupt setting failed
 *         - RET_OK                on success
 */
uint8_t Logger_datardy_int(uint8_t value);

LoggerState_t LoggerGetState();
uint8_t Logger_start();
uint8_t Logger_stop();
// uint8_t Logger_flush_buffer_to_sd_card();
uint8_t Logger_flush_buffer_to_sd_card_uint8(uint8_t * buffer, size_t size);
uint8_t Logger_flush_buffer_to_sd_card_int32(int32_t * buffer, size_t size);

uint8_t Logger_isLogging(void);
uint8_t Logger_setCsvLog(log_mode_t value);
uint8_t Logger_getCsvLog();
uint8_t Logger_syncSettings();
uint8_t Logger_sendSTM32cmd(stm32cmd_t cmd);

#endif