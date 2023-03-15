#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdint.h>
#include <stddef.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "common.h"

#define MAX_WIFI_SSID_LEN 50
#define MAX_WIFI_PASSW_LEN 20

#define NUM_ADC_CHANNELS 8

typedef enum adc_channel_e {
	ADC_CHANNEL_0 = 0x00,
	ADC_CHANNEL_1 = 0x01,
	ADC_CHANNEL_2 = 0x02,
	ADC_CHANNEL_3 = 0x03,
	ADC_CHANNEL_4 = 0x04,
	ADC_CHANNEL_5 = 0x05,
	ADC_CHANNEL_6 = 0x06,
	ADC_CHANNEL_7 = 0x07

} adc_channel_t;

typedef enum adc_channel_range_e
{
	ADC_RANGE_10V = 0,
	ADC_RANGE_60V = 1
} adc_channel_range_t;

typedef enum adc_channel_type_e
{
	ADC_CHANNEL_TYPE_NTC = 0,
	ADC_CHANNEL_TYPE_AIN = 1
} adc_channel_type_t;

typedef enum adc_resolution_e {
    ADC_12_BITS = 12,
    ADC_16_BITS = 16
} adc_resolution_t;

typedef enum adc_channel_enable_e {
	ADC_CHANNEL_DISABLED = 0,
	ADC_CHANNEL_ENABLED = 1
} adc_channel_enable_t;

typedef enum adc_sample_rate_e {
    ADC_SAMPLE_RATE_1Hz = 0,
	ADC_SAMPLE_RATE_2Hz,
	ADC_SAMPLE_RATE_5Hz,
	ADC_SAMPLE_RATE_10Hz,
	ADC_SAMPLE_RATE_25Hz,
	ADC_SAMPLE_RATE_50Hz,
	ADC_SAMPLE_RATE_100Hz,
	ADC_SAMPLE_RATE_250Hz,
	ADC_SAMPLE_RATE_500Hz,
	ADC_SAMPLE_RATE_1000Hz,
	ADC_SAMPLE_RATE_2500Hz,
	// ADC_SAMPLE_RATE_5000Hz,
	// ADC_SAMPLE_RATE_10000Hz,
    ADC_SAMPLE_RATE_NUM_ITEMS
} adc_sample_rate_t;


typedef enum log_mode_e {
    LOGMODE_RAW = 0,
    LOGMODE_CSV
} log_mode_t;

// Padding bytes added to do word alignment manually
typedef struct {
	uint8_t year;
	uint8_t month;
	uint8_t date;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
    uint8_t padding1;
	uint8_t padding2;
	uint32_t subseconds;
} s_date_time_t;


struct Settings_t {
    adc_resolution_t adc_resolution;
    adc_sample_rate_t log_sample_rate; // can make this one out of fixed options
    uint8_t adc_channel_type; // indicate whether channel 0..7 are normal ADC (bit = 0) or NTC (bit = 1). LSB = channel 0, MSB = channel 7
    uint8_t adc_channels_enabled; // Indicate whether an ADC channel should be enabled or not. Each bit represents a channel. LSB = 0 channel 0 (Mask 0x01), MSB = channel 7 (Mask 0x80)
	uint8_t adc_channel_range; // Indicate what the range of channel 0..7 is -10V / +10 (bit = 0) or -60V / +60V (bit = 1)
	uint8_t logMode;
	char wifi_ssid[MAX_WIFI_SSID_LEN];
	char wifi_password[MAX_WIFI_PASSW_LEN];
	uint8_t wifi_channel;
	uint32_t timestamp; // time in BCD format
};

typedef struct Settings_t Settings_t;

static const char * settings_filename = "settings.json";

void settings_init();
Settings_t * settings_get();
uint8_t settings_get_adc_channel_enabled(adc_channel_t channel);
uint8_t settings_get_adc_channel_enabled_all();
esp_err_t settings_set_enabled_adc_channels(adc_channel_t channel, adc_channel_enable_t value);

uint8_t settings_get_adc_channel_type(adc_channel_t channel);
uint8_t settings_get_adc_channel_type_all();
esp_err_t settings_set_adc_channel_type(adc_channel_t channel, adc_channel_type_t value);

uint8_t settings_get_adc_channel_range(adc_channel_t channel);
uint8_t settings_get_adc_channel_range_all();
esp_err_t settings_set_adc_channel_range(adc_channel_t channel, adc_channel_range_t value);

esp_err_t settings_set_default();

log_mode_t settings_get_logmode();
esp_err_t settings_set_logmode(log_mode_t mode);

uint8_t settings_get_wifi_channel();
esp_err_t settings_set_wifi_channel(uint8_t channel);

char * settings_get_wifi_password();
esp_err_t settings_set_wifi_password(char *password);

char * settings_get_wifi_ssid();
esp_err_t settings_set_wifi_ssid(char * ssid);

adc_sample_rate_t settings_get_samplerate(void);
esp_err_t settings_set_samplerate(adc_sample_rate_t rate);

esp_err_t settings_load_persisted_settings();
esp_err_t settings_persist_settings();

esp_err_t settings_print();

adc_resolution_t settings_get_resolution();
esp_err_t settings_set_resolution(adc_resolution_t res);

/// @brief Sets the current date and time based on the epoch timestamp
/// @param timestamp 32-bit Unix epoch timestamp
/// @return ESP_OK when OK and ESP_FAIL for wrong input
esp_err_t settings_set_timestamp(uint64_t timestamp);
uint32_t settings_get_timestamp();



#endif
