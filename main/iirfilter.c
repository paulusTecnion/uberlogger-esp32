#include "esp_system.h"
#include "iirfilter.h"
#include "settings.h"

// Original coefficients from https://tecnionnl.sharepoint.com/:x:/s/uberlogger/EeEoN_zLy7BHslnFgKYobd4BH9o46vYH16z9PU2SE_CJCw?e=e9fFJc
// 0.085849253
// 0.164328412
// 0.324768093
// 0.544061872
// 0.792120424
// 0.956786082
#define TAG_IIR "IIR"

#define NUM_COEFFICIENTS 7   // Filter length                                             
const int64_t c_10v[NUM_COEFFICIENTS] = {8584925, 16432841, 32476809, 54406187, 79212042, 95678608, 100000000};      // mulitplied with 100000000 
const int64_t c_60v[NUM_COEFFICIENTS] = {858493, 1643284, 3247681, 5440619, 7921204, 9567861, 10000000};            // multiplied with 10000000

int64_t * c[NUM_ADC_CHANNELS];
int64_t x_state[NUM_ADC_CHANNELS];
int64_t y_state[NUM_ADC_CHANNELS];
uint8_t coeff_index;
adc_mult_factor_t coeff_factor[NUM_ADC_CHANNELS];


void iir_filter(int32_t input, int32_t * output, uint8_t channel)
{
    // Based on the factor, we need to pick the correct coefficients 

    // Multiply and accumulate
    // ESP_LOGI(TAG_IIR, "input: %ld, coeff: %lld, y_state: %lld", input, c[channel][coeff_index], y_state[channel]);
    y_state[channel] = ((c[channel][coeff_index] * (int64_t)input ) + ((coeff_factor[channel]-c[channel][coeff_index]) * y_state[channel]))/ coeff_factor[channel];

    // the factor 1000000 is used 
    *output = (int32_t)(y_state[channel]);
}

void iir_reset()
{
    // Clear state
    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        y_state[i] = 0;
    }

}

esp_err_t iir_set_settings(adc_sample_rate_t rate, adc_channel_range_t* ranges)
{
    if (rate <= ADC_SAMPLE_RATE_50Hz)
    {
        // Bit dirty, but turns out that the enum values are the same as the index
        coeff_index = rate;
    } else {
        // no need for iir when > 50 Hz
        coeff_index = NUM_COEFFICIENTS;
        // return ESP_OK;
    }


    for (int i=0; i<NUM_ADC_CHANNELS; i++)
    {
        switch (ranges[i])
        {
            case ADC_RANGE_10V:
                coeff_factor[i] = ADC_MULT_FACTOR_10V;
                c[i] = c_10v;
            break;

            case ADC_RANGE_60V:
                coeff_factor[i] = ADC_MULT_FACTOR_60V;
                c[i] = c_60v;
            break;

            default:
                return ESP_ERR_INVALID_ARG;
        }    
        ESP_LOGI(TAG_IIR, "%d, coeff index: %d, %s, %s", i, coeff_index, 
        (coeff_factor[i] == ADC_MULT_FACTOR_10V ? "ADC_MULT_10V" : "ADC_MULT_60V"),
        (c[i] == c_10v ? "c_10v" : "c_60v"));
    }

    
    return ESP_OK;
    
}

int32_t iir_get_mult_factor (adc_channel_t channel)
{
    return (int32_t)coeff_factor[channel];
}
