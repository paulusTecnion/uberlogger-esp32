#include "settings.h"
#include "spiffs_settings.h"

static const char* TAG_SETTINGS = "SETTINGS";
Settings_t _settings;

void settings_init()
{
    if (settings_load_persisted_settings() != ESP_OK)
    {
        settings_set_default();
        settings_persist_settings();
    }
    
}

uint8_t settings_get_adc_channel_enabled(adc_channel_t channel)
{
    return _settings.adc_channels_enabled & (0x01 << channel);
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


uint8_t settings_get_adc_channel_type(adc_channel_t channel)
{
    return _settings.adc_channel_type & (0x01 << channel);
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

uint8_t settings_get_adc_channel_range(adc_channel_t channel)
{
    return _settings.adc_channel_range & (0x01 << channel);
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

esp_err_t settings_set_default()
{
    ESP_LOGI(TAG_SETTINGS, "Setting default settings");
    _settings.adc_resolution = ADC_12_BITS;
    _settings.log_sample_rate = ADC_SAMPLE_RATE_10Hz; // 10Hz 
    _settings.adc_channel_type = 0x00; // all channels normal ADC by default
    _settings.adc_channels_enabled = 0xFF; // all channels are enabled by default
    _settings.adc_channel_range = 0x00; // 10V by default
    _settings.logMode = LOGMODE_CSV;
    strcpy(_settings.wifi_ssid, "Uberlogger");
    strcpy(_settings.wifi_password, "");
    _settings.wifi_channel = 1;

    return ESP_OK;
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
        ESP_LOGI(TAG_SETTINGS, "Logmode set to CSV");
    } else if (mode == LOGMODE_RAW) {
        ESP_LOGI(TAG_SETTINGS, "Logmode set to RAW");
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
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
    
        ESP_LOGI(TAG_SETTINGS, "ADC RESOLUTION = %d", _settings.adc_resolution);
        
    return ESP_OK;    
       
}

adc_resolution_t settings_get_resolution()
{
    return _settings.adc_resolution;
}



esp_err_t settings_set_samplerate(adc_sample_rate_t rate)
{
    if (rate > 0 && rate < ADC_SAMPLE_RATE_NUM_ITEMS)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC SAMPLE RATE= %d", rate);
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
    ESP_LOGI(TAG_SETTINGS, "Loading persisted settings");
    if (spiffs_init(settings_filename) == ESP_OK)
    {
        if (spiffs_read((char*)&_settings, sizeof(_settings)) == ESP_OK)
        {
            ESP_LOGI(TAG_SETTINGS, "Persisted settings loaded succesfully");
            return ESP_OK;     
        } else {
            ESP_LOGE(TAG_SETTINGS, "Error reading settings file");
        }
    }
    ESP_LOGE(TAG_SETTINGS, "Loading persisted settings FAILED");
    return ESP_FAIL;
}

esp_err_t settings_persist_settings()
{
    if ( spiffs_write((const char*)&_settings, sizeof(_settings)) == ESP_OK)
    {
        ESP_LOGI(TAG_SETTINGS, "Settings persisted");
        return ESP_OK;     
    }
    ESP_LOGE(TAG_SETTINGS, "Persisting settings FAILED");
    return ESP_FAIL;
}