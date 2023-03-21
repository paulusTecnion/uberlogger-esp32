#include "esp_system.h"
#include "iirfilter.h"

#define Q 16

#define NUMER_COEFFICIENTS 6   // Filter length
const int32_t c[NUMER_COEFFICIENTS] = {889719, 1715413, 3387829, 5671618, 8223192, 9962013};  // Coefficients in Q21.11 format

int32_t x_state;
int32_t y_state = 0;

// default length is 70 (for 1 Hz sample rate)
uint8_t filter_length = 70;

int32_t iir_filter(int16_t input)
{
    int i;

    // Normalize input to range -1.0 to 1.0 in Q15.16 format
    int32_t x = ((int32_t)input << 16) / 32768;

    // Perform filtering
    for (i = 0; i < filter_length; i++)
    {
        // Multiply and accumulate using Q21.11 format
        y_state = ((c[i] * x_state) >> 11) + ((c[i] * y_state) >> 11);

        // Update state
        x_state = (i == 0) ? x_state : x_state;;
    }

    // Normalize output to range -1.0 to 1.0 in Q15.16 format
    int32_t output = (y_state * 32768) >> 16;

    return (int16_t)output;
}

void iir_reset()
{
    // Clear state
    x_state = 0;
    y_state = 0;
}

esp_err_t iir_set_filter_length(uint8_t length)
{
    if (length > 70 || length < 2)
    {
        return ESP_FAIL;
    }

    
    filter_length = length;
    return ESP_OK;
}