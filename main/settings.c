#include "settings.h"

static const char* TAG_SETTINGS = "SETTINGS";
Settings_t _settings;

void settings_init()
{
    _settings.adc_resolution = ADC_12_BITS;
    _settings.log_sample_rate = ADC_SAMPLE_RATE_10Hz; // 10Hz 
    _settings.adc_channel_type = 0x00; // all channels normal ADC by default
    _settings.adc_channels_enabled = 0xFF; // all channels are enabled by default
    _settings.logMode = LOGMODE_CSV;
}

uint8_t settings_get_enabled_adc_channels()
{
    return _settings.adc_channels_enabled;
}

uint8_t settings_set_enabled_adc_channels(adc_channel_t channel, uint8_t value)
{
    // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.adc_channels_enabled = _settings.adc_channels_enabled & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.adc_channels_enabled |= ((value << channel));

    return RET_OK;
}

Settings_t* settings_get(){
    return &_settings;
}

log_mode_t settings_get_logmode()
{
    return _settings.logMode;
}

uint8_t settings_set_logmode(log_mode_t mode)
{
    _settings.logMode = mode;
    if (mode == LOGMODE_CSV)
    {
        ESP_LOGI(TAG_SETTINGS, "Logmode set to CSV");
    } else if (mode == LOGMODE_RAW) {
        ESP_LOGI(TAG_SETTINGS, "Logmode set to RAW");
    } else {
        return RET_NOK;
    }
    return RET_OK;
}

uint8_t settings_set_resolution(adc_resolution_t res)
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
        return RET_NOK;
    }
    
        ESP_LOGI(TAG_SETTINGS, "ADC RESOLUTION = %d", _settings.adc_resolution);
        
    return RET_OK;    
       
}

adc_resolution_t settings_get_resolution()
{
    return _settings.adc_resolution;
}



uint8_t settings_set_samplerate(adc_sample_rate_t rate)
{
    if (rate > 0 && rate < ADC_SAMPLE_RATE_NUM_ITEMS)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC SAMPLE RATE= %d", rate);
        _settings.log_sample_rate = rate;
        return RET_OK;
    } else {
        return RET_NOK;
    }
    
}

adc_sample_rate_t settings_get_samplerate()
{
    return _settings.log_sample_rate;
}
