#ifndef _LOGGER_H
#define _LOGGER_H
#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "settings.h"

enum LoggerStates
{
    LOGGER_INIT = 0,
    LOGGER_IDLE,
    LOGGER_LOGGING,
    LOGGER_SETTINGS,
    LOGGER_NUM_STATES
};

typedef enum stm32cmd {
    STM32_CMD_NOP,
    STM32_CMD_SETTINGS_MODE,
    STM32_CMD_MEASURE_MODE,
    STM32_CMD_SET_RESOLUTION,
    STM32_CMD_SET_SAMPLE_RATE
} stm32cmd_t;

typedef enum stm32resp {
    STM32_RESP_OK = 0,
    STM32_RESP_NOK
 } stm32resp_t;

typedef uint8_t LoggerState;

LoggerState LoggerGetState();
uint8_t Logger_start();
uint8_t Logger_stop();
uint8_t Logger_setCsvLog(log_mode_t value);
uint8_t Logger_getCsvLog();
uint8_t Logger_syncSettings();
uint8_t Logger_sendSTM32cmd(stm32cmd_t cmd);

#endif