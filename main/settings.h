#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "common.h"

#define MAX_FILE_PREFIX_LENGTH 70 // max number of characters
#define MAX_FILE_SPLIT_SIZE 0x80000000 // in BYTES

#define FILE_SPLIT_SIZE_UNIT_KB 0
#define FILE_SPLIT_SIZE_UNIT_MB 1
#define FILE_SPLIT_SIZE_UNIT_GB 2

#define FILE_NAME_MODE_SEQ_NUM 0
#define FILE_NAME_MODE_TIMESTAMP 1

#define MAX_WIFI_SSID_LEN 32
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
	ADC_SAMPLE_RATE_EVERY_3600S = 0, // once per hour
	ADC_SAMPLE_RATE_EVERY_600S, // once per 10 min
	ADC_SAMPLE_RATE_EVERY_300S, // once per 5 min
	ADC_SAMPLE_RATE_EVERY_60S, // once per 1 min
	ADC_SAMPLE_RATE_EVERY_10S, // once per 10 sec
    ADC_SAMPLE_RATE_1Hz,
	ADC_SAMPLE_RATE_2Hz,
	ADC_SAMPLE_RATE_5Hz,
	ADC_SAMPLE_RATE_10Hz,
	ADC_SAMPLE_RATE_25Hz,
	ADC_SAMPLE_RATE_50Hz,
	ADC_SAMPLE_RATE_100Hz,
	ADC_SAMPLE_RATE_250Hz,
	// ADC_SAMPLE_RATE_500Hz,
	// ADC_SAMPLE_RATE_1000Hz,
	// ADC_SAMPLE_RATE_2500Hz,
	// ADC_SAMPLE_RATE_5000Hz,
	// ADC_SAMPLE_RATE_10000Hz,
    ADC_SAMPLE_RATE_NUM_ITEMS
} adc_sample_rate_t;

// Multiply offsets and coefficients with 10000000 to work with these factors
typedef enum int32 {
	ADC_12_BITS_10V_FACTOR = 488400,
	ADC_12_BITS_60V_FACTOR = 293040,
	ADC_16_BITS_10V_FACTOR = 30518, 
	ADC_16_BITS_60V_FACTOR = 18310	
} adc_factors_t;


typedef enum file_separator_char_e {
	FILE_SEPARATOR_CHAR_COMMA = 0,
	FILE_SEPARATOR_CHAR_SEMICOLON
} file_separator_char_t;

typedef enum file_decimal_character_e
{
	FILE_DECIMAL_CHAR_DOT = 0,
	FILE_DECIMAL_CHAR_COMMA = 1
} file_decimal_character_t;

typedef enum int64 {
	ADC_MULT_FACTOR_10V =	  	10000000LL,
	ADC_MULT_FACTOR_60V = 		1000000LL,
	ADC_MULT_FACTOR_16B_TEMP = 	10, // Only used for temperature values in 16 bit 
	ADC_MULT_FACTOR_12B_TEMP = 	10 // Only used for temperature values in 12 bit
} adc_mult_factor_t;

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
    adc_sample_rate_t adc_log_sample_rate; // can make this one out of fixed options
    uint8_t adc_channel_type; // indicate whether channel 0..7 are normal ADC (bit = 0) or NTC (bit = 1). LSB = channel 0, MSB = channel 7
    uint8_t adc_channels_enabled; // Indicate whether an ADC channel should be enabled or not. Each bit represents a channel. LSB = 0 channel 0 (Mask 0x01), MSB = channel 7 (Mask 0x80)
	uint8_t adc_channel_range; // Indicate what the range of channel 0..7 is -10V / +10 (bit = 0) or -60V / +60V (bit = 1)
	uint8_t averageSamples; // whether to average samples using iir filter or not (for sample rate < 1 Hz)
	uint8_t logMode;
	char wifi_ssid[MAX_WIFI_SSID_LEN];
	char wifi_ssid_ap[MAX_WIFI_SSID_LEN];
	char wifi_password[MAX_WIFI_PASSW_LEN];
	uint8_t wifi_channel;
	uint8_t wifi_mode;
	uint32_t timestamp; // time in BCD format
	// ADC 12 bit offset value in absolute value
	int32_t adc_offsets_12b[NUM_ADC_CHANNELS];
	// ADC 16 bit offset value in absolute value
	int32_t adc_offsets_16b[NUM_ADC_CHANNELS];
	// ADC temp offset value in absolute value
	int16_t temp_offsets[NUM_ADC_CHANNELS];
	uint8_t bootReason;
	/* NEW SETTINGS */
	uint8_t gpio_channels_enabled;
	file_decimal_character_t file_decimal_char; // character for decimal notation in CSVs. 0 = dot, 1 = comma
	uint8_t file_name_mode;  // 0 = sequential logfile, 1 = timestamp
	char file_prefix[MAX_FILE_PREFIX_LENGTH];
	file_separator_char_t file_separator_char; // 0 = comma, 1 = semicolon
	// File split size in bytes
	uint32_t file_split_size;
	// File split size unit. 0 = KB, 1 = MB, 2 = GB
	uint8_t file_split_size_unit;
};

struct Settings_old_t {
    adc_resolution_t adc_resolution;
    adc_sample_rate_t log_sample_rate; // can make this one out of fixed options
    uint8_t adc_channel_type; // indicate whether channel 0..7 are normal ADC (bit = 0) or NTC (bit = 1). LSB = channel 0, MSB = channel 7
    uint8_t adc_channels_enabled; // Indicate whether an ADC channel should be enabled or not. Each bit represents a channel. LSB = 0 channel 0 (Mask 0x01), MSB = channel 7 (Mask 0x80)
	uint8_t adc_channel_range; // Indicate what the range of channel 0..7 is -10V / +10 (bit = 0) or -60V / +60V (bit = 1)
	uint8_t logMode;
	char wifi_ssid[MAX_WIFI_SSID_LEN];
	char wifi_ssid_ap[MAX_WIFI_SSID_LEN];
	char wifi_password[MAX_WIFI_PASSW_LEN];
	uint8_t wifi_channel;
	uint8_t wifi_mode;
	uint32_t timestamp; // time in BCD format
	// ADC 12 bit offset value in absolute value
	int32_t adc_offsets_12b[NUM_ADC_CHANNELS];
	// ADC 16 bit offset value in absolute value
	int32_t adc_offsets_16b[NUM_ADC_CHANNELS];
	// ADC temp offset value in absolute value
	int16_t temp_offsets[NUM_ADC_CHANNELS];
	uint8_t bootReason;
};

typedef struct Settings_old_t Settings_old_t;

typedef struct Settings_t Settings_t;

void settings_init();
Settings_t * settings_get();

uint8_t settings_get_adc_channel_enabled(adc_channel_t channel);
uint8_t settings_get_adc_channel_enabled_all();
esp_err_t settings_set_adc_channel_enabled(adc_channel_t channel, adc_channel_enable_t value);

uint8_t settings_get_adc_channel_type(Settings_t *settings, adc_channel_t channel);
uint8_t settings_get_adc_channel_type_all();
esp_err_t settings_set_adc_channel_type(adc_channel_t channel, adc_channel_type_t value);

uint8_t settings_get_adc_channel_range(Settings_t *settings, adc_channel_t channel);
uint8_t settings_get_adc_channel_range_all();
esp_err_t settings_set_adc_channel_range(adc_channel_t channel, adc_channel_range_t value);

esp_err_t settings_clear_bootreason();
uint8_t settings_get_boot_reason();
uint8_t settings_set_boot_reason(uint8_t reason);

int32_t * settings_get_temp_offsets();
esp_err_t settings_set_temp_offset(int32_t * offsets);

int32_t * settings_get_adc_offsets();
esp_err_t settings_set_adc_offset(int32_t * offsets, adc_resolution_t resolution);

Settings_t settings_get_default();
esp_err_t settings_set_default();

file_decimal_character_t settings_get_file_decimal_char();
esp_err_t settings_set_file_decimal_char(file_decimal_character_t decimal_character);

file_separator_char_t settings_get_file_separator_char();
esp_err_t settings_set_file_separator(file_separator_char_t separator_character);

uint8_t settings_get_file_name_mode();
esp_err_t settings_set_file_name_mode(uint8_t mode);

esp_err_t settings_set_file_prefix(const char * prefix);
char * settings_get_file_prefix();

esp_err_t settings_set_file_split_size(uint32_t size);
uint32_t settings_get_file_split_size();

esp_err_t settings_set_file_split_size_unit(uint8_t unit);
uint8_t settings_get_file_split_size_unit();

uint8_t settings_get_gpio_channel_enabled(Settings_t *settings, uint8_t channel);
esp_err_t settings_set_gpio_channel_enabled(uint8_t channel, uint8_t value);


int8_t settings_get_last_enabled_ADC_channel();
int8_t settings_get_last_enabled_GPIO_channel();

log_mode_t settings_get_logmode();
esp_err_t settings_set_logmode(log_mode_t mode);

uint8_t settings_get_wifi_channel();
esp_err_t settings_set_wifi_channel(uint8_t channel);

char * settings_get_wifi_password();
esp_err_t settings_set_wifi_password(char *password);

char * settings_get_wifi_ssid();
esp_err_t settings_set_wifi_ssid(char * ssid);

char * settings_get_wifi_ssid_ap();

adc_sample_rate_t settings_get_samplerate(void);
esp_err_t settings_set_samplerate(adc_sample_rate_t rate);

esp_err_t settings_load_persisted_settings();
esp_err_t settings_persist_settings();

/// @brief Converts the settings to a JSON string. Requires to manually free the string from memory afterwards!
/// @param settings Pointer to settings struct
/// @return A string of the JSON output. NULL pointer on failure. 
char * settings_to_json(Settings_t *settings);

esp_err_t settings_print();

adc_resolution_t settings_get_resolution();
esp_err_t settings_set_resolution(adc_resolution_t res);

void settings_set_system_time(time_t timestamp);

/// @brief Sets the current date and time based on the epoch timestamp
/// @param timestamp 32-bit Unix epoch timestamp
/// @return ESP_OK when OK and ESP_FAIL for wrong input
esp_err_t settings_set_timestamp(uint64_t timestamp);
uint32_t settings_get_timestamp();

uint8_t settings_get_wifi_mode();
esp_err_t settings_set_wifi_mode(uint8_t mode);




#endif
