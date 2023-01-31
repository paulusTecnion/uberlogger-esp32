
#include "esp_log.h"
#include <math.h>

#define Q 16 // number of fractional bits to use for math, total int size is 32 bits

static const char * TEMP_TAG = "TEMP";
typedef struct { int32_t x; int32_t y; } lut_t;

int32_t interp( lut_t * c, int32_t x, int n );

int32_t q_mul(int32_t a, int32_t b);
int32_t q_div(int32_t a, int32_t b);

void calculateTemperatureLUT(int32_t * T, int32_t adc_out, int32_t adc_in);

// int main() {
    
//     int16_t adc_in=4095;
//     int16_t adc_out=2023;
    
//     int32_t T0, T0_float;   

//     // test LUT
//     for(adc_out=3000;adc_out>100;adc_out--){
//       T0=-1000 << Q;

//       calculateTemperatureLUT(&T0, adc_out << Q, adc_in << Q);

//       T0_float=(float)T0 / ((float) ((int32_t) (1<<Q)));
      
//     }

//     return 0;
// }


int32_t q_mul(int32_t a, int32_t b){
    int64_t result;

    result = (int64_t)a * (int64_t)b;
    result += (1<<(Q - 1)); // correction of rounding

    return result >> Q;
}

int32_t q_div(int32_t a, int32_t b){
    int64_t result;

    result=(int64_t)a << Q;
    // no rounding applied

    return (int32_t)(result / b);
}

int32_t interp( lut_t * c, int32_t x, int n ){
    int i;

    for( i = 0; i < n-1; i++ )
    {
        if ( c[i].x <= x && c[i+1].x >= x )
        {
            int32_t diffx = x - c[i].x;
            int32_t diffn = c[i+1].x - c[i].x;

            return (c[i].y << Q) + q_mul(( c[i+1].y - c[i].y ) << Q, q_div(diffx << Q, diffn << Q)); 
        }
    }
    return 0; // Not in range
}


void calculateTemperatureLUT(int32_t * T, int32_t adc_out, int32_t adc_in){
  /* calculate temperature with NTC and pull-up
   * result is T in Q-notation, divide by 2^Q to get the actual value
   *
   * adc_in = ADC value of input voltage
   * adc_out = ADC value of resistor divider voltage
   *
   * calculation used:
   * R_ntc = R_pullup * (V_o/(V_i-V_o))
   */
  

  #define NTC_R_NOMINAL 100000 //.0e3 // NTC resistance at 25 deg C
  #define NTC_R_PULLUP 100000 //e3 // pullup resistance
  #define TEMPERATURE_FILTER_COEFF 0.1 // IIR filter coefficient

  #define NTC_LUT_SIZE 30
  lut_t NTC_LUT[NTC_LUT_SIZE] = { // LUT is not in Q notation, is converted in the interpolation function
      {387, 120},
      {443, 115},
      {508, 110},
      {584, 105},
      {674, 100},
      {782, 95},
      {909, 90},
      {1062, 85},
      {1246, 80},
      {1473, 75},
      {1738, 70},
      {2066, 65},
      {2468, 60},
      {2963, 55},
      {3588, 50},
      {4341, 45},
      {5298, 40},
      {6504, 35},
      {8034, 30},
      {10000, 25},
      {12499, 20},
      {15751, 15},
      {19993, 10},
      {25570, 5},
      {32960, 0},
      {42834, -5},
      {56142, -10},
      {74238, -15},
      {99077, -20},
      {133500, -25}
    };

  int32_t R, result;
  
  if((adc_out < 0.93*adc_in)&&(adc_out > 0.039*adc_in)){ // sensor has valid range (between -20 deg C and +120 deg C)
    
    R = q_mul((int32_t) (NTC_R_PULLUP << Q), q_div(adc_out, adc_in - adc_out)) >> Q;  
    result=interp(NTC_LUT, R, NTC_LUT_SIZE);

    if(result > (*T + 100)){ // detect re-connection of temperature sensor, initialize
      *T = result;
    }else{
      *T = (1 - TEMPERATURE_FILTER_COEFF) * (*T) + TEMPERATURE_FILTER_COEFF * result;
    }
    
  } else {
    *T = -1000 << Q; // sensor has invalid reading
  }
}

void calculateTemperatureFloat(float* T, float v_out, float v_in){
  // calculate temperature with 10k NTC with 10k pull-up from measured voltages v_out and v_in of resistor divider
  
  // R_ntc = R_pullup * (V_o/(V_i-V_o))
  // T_ntc = 1/((log(R_ntc/R_ntc_nominal)/NTC_BETA)+1/(25.0+273.15))-273.15
  
  #define NTC_BETA 3950 // NTC beta
  #define NTC_R_NOMINAL 10.0e3 // NTC resistance at 25 deg C
  #define NTC_R_PULLUP 10.0e3 // pullup resistance

  float R, steinhart;
  
  if((v_out < 0.91*v_in)&&(v_out > 0.08*v_in)){ // sensor has valid range (between -20 deg C and +90 deg C)
    
    R = NTC_R_PULLUP * (v_out / (v_in - v_out));
    
    steinhart = R / NTC_R_NOMINAL;
    steinhart = log(steinhart);
    steinhart /= NTC_BETA;
    steinhart += 1.0 / (25.0 + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15;

    // if(steinhart > (*T + 100)){ // detect re-connection of temperature sensor, initialize
    //   *T = steinhart;
    // }else{
    //   *T = (1 - TEMPERATURE_FILTER_COEFF) * (*T) + TEMPERATURE_FILTER_COEFF * steinhart;
    // }

      // ESP_LOGI(TEMP_TAG, "T %f", T);
    *T = steinhart;
    
  } else {
    *T = -1e3; // sensor has invalid reading
  }
}