/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#pragma once

#include <stdint.h>

void calculateTemperatureLUT(int32_t * T, int32_t adc_out, int32_t adc_in);
// void calculateTemperatureFloat(float* T, float v_out, float v_in);
int NTC_ADC2Temperature(unsigned int adc_value);
