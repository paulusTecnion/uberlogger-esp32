/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
//#include "esp_system.h"
#include <stdint.h>
#include "iirfilter.h"
#include "settings.h"


#define FIXEDPT_WBITS 16
#define Q15_SHIFT 15  // Shift amount for Q15 format
#include "fixedptc.h"


#define NUM_COEFFICIENTS 8   // Filter length
#define NUM_ADC_CHANNELS 8

 fixedptu cfl[NUM_COEFFICIENTS];

//int64_t x_state[NUM_ADC_CHANNELS];

uint8_t coeff_index = 0;

//fixedpt cfp, input_fp, output_fp;
fixedptu cfp;

static uint16_t round_value_16b(uint32_t val) {
    return (val + (1<<15)) >> 16;  // Add half of 2^16 to round, then shift
}

static uint16_t round_value_12b(uint32_t val) {
    return (val + (1<<11)) >> 12;  // Add half of 2^12 to round, then shift
}

fixedptu temp[NUM_ADC_CHANNELS] = {0};

void iir_filter_16b(uint16_t * input, uint16_t * output, uint8_t channel)
{
	// Here we calculate the error in 32 bits. Then we add a fraction of the error to the output of signal. This way we smartly solve the limit cycle problem with iir filters.
	// See: https://dsp.stackexchange.com/questions/66171/single-pole-iir-filter-fixed-point-design
	 // Convert input and output to fixed-point
	uint32_t input_32 = (fixedptu)(*input);
	uint32_t output_32= (fixedptu)(*output);
    fixedptu input_fp = (input_32);
    fixedptu output_fp = (output_32);
	
    // Calculate error and adjustment in fixed-point
    temp[channel] =  temp[channel] + cfp*(input_fp-output_fp);
	
	fixedptu t;
    t= round_value_16b(temp[channel]);
	*output = (uint16_t)t;
}


void iir_filter_12b(uint16_t * input, uint16_t * output, uint8_t channel)
{
	// Here we calculate the error in 32 bits. Then we add a fraction of the error to the output of signal. This way we smartly solve the limit cycle problem with iir filters.
	// See: https://dsp.stackexchange.com/questions/66171/single-pole-iir-filter-fixed-point-design
	uint32_t input_32 = (fixedptu)(*input);
	uint32_t output_32= (fixedptu)(*output);
    fixedptu input_fp = (input_32);
    fixedptu output_fp = (output_32);
	
    // Calculate error and adjustment in fixed-point
    temp[channel] =  temp[channel] + cfp*(input_fp-output_fp);
	
	fixedptu t;
    t= round_value_12b(temp[channel] >> 3); // Need to shift 3 to right, else we don't get the correct conversion to ints again. 
	*output = (uint16_t)t;
}

// void iir_reset()
// {
//     // Clear state
//     for (int i = 0; i < NUM_ADC_CHANNELS; i++)
//     {
//         y_state[i] = 0;
//     }

// }

uint8_t iir_set_samplefreq(uint8_t sampleFreq)
{
	if (	sampleFreq >= ADC_SAMPLE_RATE_EVERY_3600S &&
			sampleFreq < ADC_SAMPLE_RATE_1Hz)
	{
		coeff_index = sampleFreq;
		cfp = cfl[coeff_index];

		return 0;
	}

	return 1;

}

void iir_init()
{
	// Original coefficients from Excel sheet. See internal documentation.
	// 0.000069811
	// 0.000418791
	// 0.000837407
	// 0.004180029
	// 0.024819543
	cfl[0] = fixedpt_rconst(0.00069811); 
	cfl[1] = fixedpt_rconst(0.000418791);
	cfl[2] = fixedpt_rconst(0.000837407);
	cfl[3] = fixedpt_rconst(0.004180029);
	cfl[4] = fixedpt_rconst(0.024819543);

}





