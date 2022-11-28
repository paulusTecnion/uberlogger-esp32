#include "settings.h"

Settings_t _settings;

void settings_init()
{
    _settings.adc_resolution = ADC_12_BITS;
    _settings.log_sample_rate = 1; // 1 Hz 
    _settings.adc_channel_type = 0x00; // all channels normal ADC by default

}

Settings_t* settings_get(){
    return &_settings;
}