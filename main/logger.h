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


typedef uint8_t LoggerState;

LoggerState LoggerGetState();
uint8_t Logger_start();
uint8_t Logger_stop();
uint8_t Logger_setCsvLog(log_mode_t value);
uint8_t Logger_getCsvLog();
uint8_t Logger_syncSettings();

#endif