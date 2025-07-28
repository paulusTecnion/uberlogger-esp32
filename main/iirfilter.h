/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#ifndef __IIRFILTER_H
#define __IIRFILTER_H

/// @brief 
/// @param input 
/// @return 
void  iir_filter_16b(uint16_t * input, uint16_t * output, uint8_t channel);
void iir_filter_12b(uint16_t * input, uint16_t * output, uint8_t channel);
uint8_t iir_set_samplefreq(uint8_t sampleFreq);
// void iir_reset();
void iir_init();

#endif
