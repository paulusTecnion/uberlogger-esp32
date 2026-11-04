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

// Valid range of the NTC lookup table, in 0.1 degC. Outside it the sensor is
// open (reads cold) or shorted (reads hot), or simply out of spec.
#define NTC_TEMP_MIN (-400)
#define NTC_TEMP_MAX 1200
// Returned for a reading outside the valid range. Written as an empty CSV
// field and as null in the live JSON. Same value as CSV_INVALID_VALUE.
#define NTC_TEMP_INVALID INT32_MIN

// Convert an offset-compensated ADC value of the given resolution (12 or 16
// bits) to a temperature in 0.1 degC, or NTC_TEMP_INVALID.
int32_t NTC_ADC2Temperature(int32_t adc_value, uint8_t resolution_bits);
