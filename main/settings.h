#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdint.h>
#include <stddef.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "common.h"

typedef enum adc_channel_e {
	ADC_CHANNEL_0 = 0,
	ADC_CHANNEL_1,
	ADC_CHANNEL_2,
	ADC_CHANNEL_3,
	ADC_CHANNEL_4,
	ADC_CHANNEL_5,
	ADC_CHANNEL_6,
	ADC_CHANNEL_7

} adc_channel_t;

typedef enum adc_resolution_e {
    ADC_12_BITS = 1,
    ADC_16_BITS = 2,
    ADC_RESOLUTION_NUM_ITEMS
} adc_resolution_t;

typedef enum adc_sample_rate_e {
    ADC_SAMPLE_RATE_1Hz = 1,
	ADC_SAMPLE_RATE_10Hz,
	ADC_SAMPLE_RATE_25Hz,
	ADC_SAMPLE_RATE_50Hz,
	ADC_SAMPLE_RATE_100Hz,
	ADC_SAMPLE_RATE_200Hz,
	ADC_SAMPLE_RATE_400Hz,
	ADC_SAMPLE_RATE_500Hz,
	ADC_SAMPLE_RATE_1000Hz,
	ADC_SAMPLE_RATE_2000Hz,
	ADC_SAMPLE_RATE_4000Hz,
	ADC_SAMPLE_RATE_5000Hz,
	ADC_SAMPLE_RATE_8000Hz,
	ADC_SAMPLE_RATE_10000Hz,
	ADC_SAMPLE_RATE_20000Hz,
	ADC_SAMPLE_RATE_40000Hz,
	ADC_SAMPLE_RATE_50000Hz,
	ADC_SAMPLE_RATE_100000Hz,
	ADC_SAMPLE_RATE_250000Hz,
	ADC_SAMPLE_RATE_500000Hz,
	ADC_SAMPLE_RATE_1000000Hz,
    ADC_SAMPLE_RATE_NUM_ITEMS
} adc_sample_rate_t;


typedef enum log_mode_e {
    LOGMODE_RAW = 0,
    LOGMODE_CSV
} log_mode_t;


struct Settings_t {
    adc_resolution_t adc_resolution;
	uint8_t adc_resolution_uint8;
    adc_sample_rate_t log_sample_rate; // can make this one out of fixed options
    uint8_t adc_channel_type; // indicate whether channel 0..7 are normal ADC (bit = 0) or NTC (bit = 1). LSB = channel 0, MSB = channel 7
    uint8_t adc_channels_enabled; // Indicate whether an ADC channel should be enabled or not. Each bit represents a channel. LSB = 0 channel 0 (Mask 0x01), MSB = channel 7 (Mask 0x80)
	uint8_t logMode;
};

typedef struct Settings_t Settings_t;



void settings_init();
Settings_t * settings_get();
uint8_t settings_get_enabled_adc_channels();
log_mode_t settings_get_logmode();
adc_resolution_t settings_get_resolution();
uint8_t settings_get_resolution_uint8();
uint8_t settings_set_resolution(adc_resolution_t res);
uint8_t settings_set_logmode(log_mode_t mode);
uint8_t settings_set_samplerate(adc_sample_rate_t rate);
adc_sample_rate_t settings_get_samplerate(void);


#endif
