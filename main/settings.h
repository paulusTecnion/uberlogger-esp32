#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdint.h>
#include <stddef.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "common.h"

enum adc_resolution_e {
    ADC_12_BITS = 0,
    ADC_16_BITS = 1
};

typedef enum log_mode_e {
    LOGMODE_RAW = 0,
    LOGMODE_CSV
} log_mode_t;


struct Settings_t {
    uint8_t adc_resolution;
    uint32_t log_sample_rate; // can make this one out of fixed options
    uint8_t adc_channel_type; // indicate whether channel 0..7 are normal ADC (bit = 0) or NTC (bit = 1). LSB = channel 0, MSB = channel 7
    uint8_t logMode;
};

typedef struct Settings_t Settings_t;



void settings_init();
Settings_t * settings_get();
log_mode_t settings_get_logmode();
uint8_t settings_set_logmode(log_mode_t mode);

#endif
