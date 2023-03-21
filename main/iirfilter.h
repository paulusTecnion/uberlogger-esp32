#ifndef __IIRFILTER_H
#define __IIRFILTER_H

#include <stdio.h>
#include <stdint.h>

/// @brief 
/// @param input 
/// @return 
int32_t iir_filter(int16_t input);

esp_err_t iir_set_filter_length(uint8_t length);
void iir_reset();

#endif