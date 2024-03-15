#include "settings.h"
#include "rest_server.h"
#include "spiffs_settings.h"
#include "esp_wifi_types.h"
#include "wifi.h"
static const char* TAG_SETTINGS = "SETTINGS";
const char * settings_filename = "settings.json";
Settings_t _settings;

// Mult factor for the ADC channels.
// Use as follows: mult_factor[resolution][range]
// int32_t adc_factor[2][2];
// int64_t adc_mult_factor[2];

void settings_init()
{
    if (settings_load_persisted_settings() != ESP_OK)
    {
        settings_set_default();
        settings_persist_settings();
    }

}

uint8_t settings_get_adc_channel_enabled(Settings_t *settings, adc_channel_t channel)
{
    return settings->adc_channels_enabled & (0x01 << channel);
}

esp_err_t settings_set_adc_channel_enabled(adc_channel_t channel, adc_channel_enable_t value)
{
    // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.adc_channels_enabled = _settings.adc_channels_enabled & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.adc_channels_enabled |= ((value << channel));

    return ESP_OK;
}

uint8_t settings_get_adc_channel_enabled_all()
{
    return _settings.adc_channels_enabled;
}


uint8_t settings_get_adc_channel_type(Settings_t *settings, adc_channel_t channel)
{
    return settings->adc_channel_type & (0x01 << channel);
}

uint8_t settings_get_adc_channel_type_all()
{
    return _settings.adc_channel_type;
}

esp_err_t settings_set_adc_channel_type(adc_channel_t channel, adc_channel_type_t value)
{
     // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.adc_channel_type = _settings.adc_channel_type & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.adc_channel_type |= ((value << channel));
    return ESP_OK;
}

Settings_t* settings_get(){
    return &_settings;
}

uint8_t settings_get_adc_channel_range(Settings_t * settings, adc_channel_t channel)
{
    return settings->adc_channel_range & (0x01 << channel);
}

uint8_t settings_get_adc_channel_range_all()
{
    return _settings.adc_channel_range;
}

esp_err_t settings_set_adc_channel_range(adc_channel_t channel, adc_channel_range_t value)
{
      // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.adc_channel_range = _settings.adc_channel_range & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.adc_channel_range |= ((value << channel));

    return ESP_OK;
}

esp_err_t settings_clear_bootreason()
{
    _settings.bootReason = 0;
    if (settings_persist_settings() != ESP_OK)
    {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

uint8_t settings_get_boot_reason()
{
    return _settings.bootReason;
}

uint8_t settings_set_boot_reason(uint8_t reason)
{
    _settings.bootReason = reason;
    settings_persist_settings();
    return ESP_OK;
}

int32_t * settings_get_temp_offsets()
{
    return (int32_t*)_settings.temp_offsets;
}

esp_err_t settings_set_temp_offsets(int32_t * offsets)
{
    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        _settings.temp_offsets[i] = (uint16_t)offsets[i];
    }
    return ESP_OK;
}

int32_t * settings_get_adc_offsets_12b()
{
    return (int32_t*)_settings.adc_offsets_12b;
}

int32_t * settings_get_adc_offsets_16b()
{
    return (int32_t*)_settings.adc_offsets_16b;
}

esp_err_t settings_set_adc_offset(int32_t * offsets, adc_resolution_t resolution)
{
    if (resolution == ADC_12_BITS)
    {
        for (int i = 0; i < NUM_ADC_CHANNELS; i++)
        {
            _settings.adc_offsets_12b[i] = offsets[i];
        }
    } else if (resolution == ADC_16_BITS)
    {
        for (int i = 0; i < NUM_ADC_CHANNELS; i++)
        {
            _settings.adc_offsets_16b[i] = offsets[i];
        }
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}

Settings_t settings_get_default()
{
    Settings_t default_settings;

    default_settings.adc_resolution = ADC_12_BITS;
    default_settings.log_sample_rate = ADC_SAMPLE_RATE_10Hz; // 10Hz 
    default_settings.adc_channel_type = 0x00; // all channels normal ADC by default
    default_settings.adc_channels_enabled = 0xFF; // all channels are enabled by default
    default_settings.adc_channel_range = 0x00; // 10V by default
    default_settings.logMode = LOGMODE_CSV;
    default_settings.bootReason = 0;
    strcpy(_settings.file_prefix, "log");
    _settings.file_split_size = MAX_FILE_SPLIT_SIZE; // always in KiB.
    _settings.file_split_size_unit  = 2; // 0 = KiB, 1 = MiB, 2 = GiB
    // Get mac address
    char buffer[8];
    wifi_get_trimmed_mac(buffer);
    
    sprintf(default_settings.wifi_ssid_ap, "Uberlogger-%s", buffer);
    
    
    strcpy(default_settings.wifi_ssid, default_settings.wifi_ssid_ap);
    strcpy(default_settings.wifi_password, "");
    default_settings.wifi_mode = WIFI_MODE_AP;
    default_settings.wifi_channel = 1;

    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        default_settings.adc_offsets_12b[i] = (1<<11);
        default_settings.adc_offsets_16b[i] = (1<<15);
        default_settings.temp_offsets[i] = 0;
    }

    return default_settings;
}

esp_err_t settings_set_default()
{
    #ifdef DEBUG_SETTINGS
    ESP_LOGI(TAG_SETTINGS, "Setting default settings");
    #endif
    _settings.adc_resolution = ADC_12_BITS;

    _settings.log_sample_rate = ADC_SAMPLE_RATE_10Hz; // 10Hz 
    _settings.adc_channel_type = 0x00; // all channels normal ADC by default
    _settings.adc_channels_enabled = 0xFF; // all channels are enabled by default
    _settings.adc_channel_range = 0x00; // 10V by default
    _settings.logMode = LOGMODE_CSV;
    _settings.bootReason = 0;
    strcpy(_settings.file_prefix, "log");
    _settings.file_split_size = MAX_FILE_SPLIT_SIZE; // always in KiB.
    _settings.file_split_size_unit  = 2; // 0 = KiB, 1 = MiB, 2 = GiB
    // Get mac address
    char buffer[8];
    wifi_get_trimmed_mac(buffer);
    
    sprintf(_settings.wifi_ssid_ap, "Uberlogger-%s", buffer);
    
    
    strcpy(_settings.wifi_ssid, _settings.wifi_ssid_ap);
    strcpy(_settings.wifi_password, "");
    _settings.wifi_mode = WIFI_MODE_AP;
    _settings.wifi_channel = 1;

    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        _settings.adc_offsets_12b[i] = (1<<11);
        _settings.adc_offsets_16b[i] = (1<<15);
        _settings.temp_offsets[i] = 0;
    }

    return ESP_OK;
}


esp_err_t settings_set_file_prefix(const char * prefix)
{
    if (strlen(prefix) > (MAX_FILE_PREFIX_LENGTH - 1))
        return ESP_ERR_INVALID_SIZE;
    
    strcpy(_settings.file_prefix, prefix);
    return ESP_OK;
}

char * settings_get_file_prefix()
{
    return _settings.file_prefix;
}

esp_err_t settings_set_file_split_size(uint32_t size)
{

    uint64_t sizeInKiB = 0;

    switch (_settings.file_split_size_unit)
    {
        case 0:
            // KiB
           sizeInKiB = size;
        break;

        case 1:
            // MiB
            sizeInKiB = size * 1024;
        break;

        case 2:
            // GiB

            sizeInKiB = size * 1024 * 1024; // From GiB to KiB
        break;

        default:
        return ESP_ERR_INVALID_ARG;
    }

    if (sizeInKiB < (200) ||
        (sizeInKiB > MAX_FILE_SPLIT_SIZE ))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    
    _settings.file_split_size = sizeInKiB*1024;

    
    return ESP_OK;
}

// Returns the file split size in BYTES
uint32_t settings_get_file_split_size()
{
    return _settings.file_split_size;
}

esp_err_t settings_set_file_split_size_unit(uint8_t unit)
{
    if (unit > 2)
        return ESP_ERR_INVALID_ARG;
    _settings.file_split_size_unit = unit;    
    return ESP_OK;
}

uint8_t settings_get_file_split_size_unit()
{
    return _settings.file_split_size_unit;
}


log_mode_t settings_get_logmode()
{
    return _settings.logMode;
}

esp_err_t settings_set_logmode(log_mode_t mode)
{
    _settings.logMode = mode;
    if (mode == LOGMODE_CSV)
    {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "Logmode set to CSV");
        #endif
    } else if (mode == LOGMODE_RAW) {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "Logmode set to RAW");
        #endif
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Set system time. Timestamp in seconds
void settings_set_system_time(time_t timestamp)
{
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0; // Set microseconds to 0, as time_t is typically accurate only to seconds.

    // Set the system time
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG_SETTINGS, "System time set ");

    // Obtain and print the current system time
    time_t now;
    struct tm *timeinfo;
    char buffer[64]; // Make sure this buffer is large enough for your chosen date-time format

    time(&now); // Get the current time
    timeinfo = localtime(&now); // Convert the current time to local time

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo); // Format the time as a string
    ESP_LOGI(TAG_SETTINGS, "Current system time: %s", buffer);
}


uint8_t settings_get_wifi_channel()
{
    return _settings.wifi_channel;
}

esp_err_t settings_set_wifi_channel(uint8_t channel)
{
    if (channel > 0 && channel < 14)
    {
        _settings.wifi_channel = channel;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

char * settings_get_wifi_password()
{
    return _settings.wifi_password;
}

esp_err_t settings_set_wifi_password(char *password)
{
    uint8_t passlen = strlen(password);
    if ( 
        (passlen == 0) ||  // password can be empty
        (passlen > 7 && passlen < MAX_WIFI_PASSW_LEN))
    {
        strcpy(_settings.wifi_password, password);
        return ESP_OK;
    }

    return ESP_FAIL;
}

char * settings_get_wifi_ssid()
{
    return _settings.wifi_ssid;
}

char * settings_get_wifi_ssid_ap()
{
    return _settings.wifi_ssid_ap;
}

esp_err_t settings_set_wifi_ssid(char * ssid)
{
    if (strlen(ssid) < MAX_WIFI_SSID_LEN)
    {
        strcpy(_settings.wifi_ssid, ssid);
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t settings_set_resolution(adc_resolution_t res)
{
    switch (res)
    {
        case ADC_12_BITS:
            _settings.adc_resolution = res;
        break;

        case ADC_16_BITS:
            _settings.adc_resolution = res;
        break;

        default:
        return ESP_FAIL;
    }
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "ADC RESOLUTION = %d", _settings.adc_resolution);
        #endif
        
    return ESP_OK;    
       
}

adc_resolution_t settings_get_resolution()
{
    return _settings.adc_resolution;
}



esp_err_t settings_set_samplerate(adc_sample_rate_t rate)
{
    if (rate >= ADC_SAMPLE_RATE_1Hz && rate < ADC_SAMPLE_RATE_NUM_ITEMS)
    {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "ADC SAMPLE RATE= %d", rate);
        #endif
        _settings.log_sample_rate = rate;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
    
}

adc_sample_rate_t settings_get_samplerate()
{
    return _settings.log_sample_rate;
}

esp_err_t settings_load_persisted_settings()
{
    #ifdef DEBUG_SETTINGS
    ESP_LOGI(TAG_SETTINGS, "Loading persisted settings");
    #endif
    if (spiffs_init(settings_filename) == ESP_OK)
    {
        if (spiffs_read((char*)&_settings, sizeof(_settings)) == ESP_OK)
        {
            #ifdef DEBUG_SETTINGS
            ESP_LOGI(TAG_SETTINGS, "Persisted settings loaded succesfully");
            #endif
            return ESP_OK;     
        } else {
            ESP_LOGE(TAG_SETTINGS, "Error reading settings file, setting and persisting defaults");
            settings_set_default();
            settings_persist_settings();
        }
    }
    ESP_LOGE(TAG_SETTINGS, "Loading persisted settings FAILED");
    return ESP_FAIL;
}

esp_err_t settings_print()
{
    int i =0;
    
    ESP_LOGI(TAG_SETTINGS, "ADC Resolution %d", _settings.adc_resolution);
    ESP_LOGI(TAG_SETTINGS, "ADC Sample rate %d", _settings.log_sample_rate);
    

    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC ch%d type: %s", i, (_settings.adc_channel_type & (1<<i)) ? "NTC" : "Analog");
    }
    
    
    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC ch%d range:%s", i, (_settings.adc_channel_range & (1<<i)) ? "+/-60V" : "+/-10V");
    }

    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC 12 bit offset %d: %ld", i, _settings.adc_offsets_12b[i]);
    }

    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC 16 bit offset %d: %ld", i, _settings.adc_offsets_16b[i]);
    }

    ESP_LOGI(TAG_SETTINGS, "Log mode: %s", _settings.logMode ? "CSV" : "RAW");
    ESP_LOGI(TAG_SETTINGS, "File prefix %s", _settings.file_prefix);
    ESP_LOGI(TAG_SETTINGS, "File split size unit: %d", _settings.file_split_size_unit);
    ESP_LOGI(TAG_SETTINGS, "File split size: %ld", _settings.file_split_size);



    ESP_LOGI(TAG_SETTINGS, "Wifi SSID %s", _settings.wifi_ssid);
    ESP_LOGI(TAG_SETTINGS, "Wifi AP SSID %s", _settings.wifi_ssid_ap);
    ESP_LOGI(TAG_SETTINGS, "Wifi channel %d", _settings.wifi_channel);
    
    return ESP_OK;
}

esp_err_t settings_persist_settings()
{
    const char * json = logger_settings_to_json(&_settings);
    FILE * f = fopen("/www/settings.json", "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG_SETTINGS, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, "%s", json);
    fclose(f);


    if ( spiffs_write((const char*)&_settings, sizeof(_settings)) == ESP_OK)
    {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "Settings persisted");
        #endif
        return ESP_OK;     
    }



    ESP_LOGE(TAG_SETTINGS, "Persisting settings FAILED");
    return ESP_FAIL;
}

esp_err_t settings_set_timestamp(uint64_t timestamp)
{
    // take timestamp and convert to day, month, year and time
    #ifdef DEBUG_SETTINGS
    ESP_LOGI(TAG_SETTINGS, "Timstamp: %d", timestamp);
    #endif
    // Expecting this in ms, but convert it to seconds
    ESP_LOGI(TAG_SETTINGS, "Incoming timestamp: %lld, outgoing %ld", timestamp, (uint32_t)(timestamp/1000));

    _settings.timestamp = (uint32_t)(timestamp/1000);
    settings_set_system_time(_settings.timestamp);
    return ESP_OK;
}

uint32_t settings_get_timestamp()
{
    return _settings.timestamp;
}

uint8_t settings_get_wifi_mode()
{
    return _settings.wifi_mode;
}

esp_err_t settings_set_wifi_mode(uint8_t mode)
{
    if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP)
    {
        _settings.wifi_mode = mode;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}