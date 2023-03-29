#ifndef __IIRFILTER_H
#define __IIRFILTER_H

#include <stdio.h>
#include <stdint.h>
#include "settings.h"


/// @brief 
/// @param input 
/// @return 
void  iir_filter(int32_t input, int32_t * output, uint8_t channel);
esp_err_t iir_set_settings(adc_sample_rate_t rate, adc_channel_range_t* ranges);
void iir_reset();
int32_t iir_get_mult_factor (adc_channel_t channel);

#endif