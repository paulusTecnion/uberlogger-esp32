#pragma once

#include <stdint.h>

void calculateTemperatureLUT(int32_t * T, int32_t adc_out, int32_t adc_in);
void calculateTemperatureFloat(float* T, float v_out, float v_in);