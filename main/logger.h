#ifndef _LOGGER_H
#define _LOGGER_H
#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum LoggerStates
{
    LOGGER_INIT = 0,
    LOGGER_IDLE,
    LOGGER_LOGGING,
    LOGGER_NUM_STATES
};

enum LoggerReturns
{
    RET_OK = 0 ,
    RET_NOK,
    RET_ERROR
};

typedef uint8_t LoggerState;

LoggerState LoggerGetState();
uint8_t Logger_start();
uint8_t Logger_stop();
uint8_t Logger_setCsvLog(uint8_t value);
uint8_t Logger_getCsvLog();

#endif