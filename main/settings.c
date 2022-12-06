#include "settings.h"

Settings_t _settings;

void settings_init()
{
    _settings.adc_resolution = ADC_12_BITS;
    _settings.log_sample_rate = ADC_SAMPLE_RATE_10Hz; // 10Hz 
    _settings.adc_channel_type = 0x00; // all channels normal ADC by default
    _settings.logMode = LOGMODE_CSV;
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

    return RET_OK;
}

uint8_t settings_set_samplerate(adc_sample_rate_t rate)
{
    if (rate > 0 && rate < ADC_SAMPLE_RATE_NUM_ITEMS)
    {
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
