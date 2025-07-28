/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */

#include "esp_log.h"
#include <math.h>
#include "tempsensor.h"
#define Q 16 // number of fractional bits to use for math, total int size is 32 bits

static const char * TEMP_TAG = "TEMP";
typedef struct { int32_t x; int32_t y; } lut_t;

int32_t interp( lut_t * c, int32_t x, int n );

int32_t q_mul(int32_t a, int32_t b);
int32_t q_div(int32_t a, int32_t b);

  #define NTC_R_NOMINAL_INT 100000 //.0e3 // NTC resistance at 25 deg C
  #define NTC_R_PULLUP_INT 100000 //e3 // pullup resistance
  #define TEMPERATURE_FILTER_COEFF 0.1 // IIR filter coefficient


  #define NTC_BETA 3950 // NTC beta
  #define NTC_R_NOMINAL 10.0e3 // NTC resistance at 25 deg C
  #define NTC_R_PULLUP 10.0e3 // pullup resistance


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

// Next table comes from following link
// https://www.sebulli.com/ntc/index.php?lang=en&points=1024&unit=0.1&resolution=12+Bit&circuit=pullup&resistor=10000&r25=10000&beta=3950&test_resistance=10000&tmin=-40&tmax=120

int NTC_table[1025] = {
4140, 3521, 2902, 2593, 2394, 2249, 2136,
2045, 1969, 1903, 1846, 1795, 1750, 1709,
1672, 1638, 1607, 1578, 1550, 1525, 1501,
1479, 1457, 1437, 1418, 1400, 1383, 1366,
1350, 1335, 1321, 1307, 1293, 1280, 1268,
1256, 1244, 1233, 1222, 1211, 1201, 1191,
1181, 1171, 1162, 1153, 1144, 1136, 1127,
1119, 1111, 1104, 1096, 1088, 1081, 1074,
1067, 1060, 1054, 1047, 1041, 1034, 1028,
1022, 1016, 1010, 1004, 999, 993, 988, 982,
977, 972, 967, 961, 956, 952, 947, 942, 937,
933, 928, 924, 919, 915, 910, 906, 902, 898,
894, 889, 885, 881, 878, 874, 870, 866, 862,
859, 855, 851, 848, 844, 841, 837, 834, 830,
827, 824, 820, 817, 814, 811, 808, 804, 801,
798, 795, 792, 789, 786, 783, 780, 777, 775,
772, 769, 766, 763, 761, 758, 755, 752, 750,
747, 745, 742, 739, 737, 734, 732, 729, 727,
724, 722, 719, 717, 715, 712, 710, 707, 705,
703, 701, 698, 696, 694, 692, 689, 687, 685,
683, 681, 678, 676, 674, 672, 670, 668, 666,
664, 662, 660, 658, 656, 654, 652, 650, 648,
646, 644, 642, 640, 638, 636, 634, 632, 630,
628, 627, 625, 623, 621, 619, 617, 616, 614,
612, 610, 609, 607, 605, 603, 602, 600, 598,
596, 595, 593, 591, 590, 588, 586, 585, 583,
581, 580, 578, 576, 575, 573, 572, 570, 569,
567, 565, 564, 562, 561, 559, 558, 556, 555,
553, 552, 550, 548, 547, 546, 544, 543, 541,
540, 538, 537, 535, 534, 532, 531, 529, 528,
527, 525, 524, 522, 521, 520, 518, 517, 515,
514, 513, 511, 510, 509, 507, 506, 505, 503,
502, 501, 499, 498, 497, 495, 494, 493, 491,
490, 489, 487, 486, 485, 484, 482, 481, 480,
479, 477, 476, 475, 473, 472, 471, 470, 469,
467, 466, 465, 464, 462, 461, 460, 459, 458,
456, 455, 454, 453, 452, 450, 449, 448, 447,
446, 444, 443, 442, 441, 440, 439, 438, 436,
435, 434, 433, 432, 431, 430, 428, 427, 426,
425, 424, 423, 422, 421, 419, 418, 417, 416,
415, 414, 413, 412, 411, 409, 408, 407, 406,
405, 404, 403, 402, 401, 400, 399, 398, 397,
396, 394, 393, 392, 391, 390, 389, 388, 387,
386, 385, 384, 383, 382, 381, 380, 379, 378,
377, 376, 375, 374, 373, 372, 371, 370, 369,
368, 367, 366, 365, 364, 363, 362, 361, 360,
359, 358, 357, 356, 355, 354, 353, 352, 351,
350, 349, 348, 347, 346, 345, 344, 343, 342,
341, 340, 339, 338, 337, 336, 335, 334, 333,
332, 331, 330, 329, 328, 327, 326, 325, 325,
324, 323, 322, 321, 320, 319, 318, 317, 316,
315, 314, 313, 312, 311, 310, 310, 309, 308,
307, 306, 305, 304, 303, 302, 301, 300, 299,
298, 298, 297, 296, 295, 294, 293, 292, 291,
290, 289, 288, 287, 287, 286, 285, 284, 283,
282, 281, 280, 279, 278, 278, 277, 276, 275,
274, 273, 272, 271, 270, 269, 269, 268, 267,
266, 265, 264, 263, 262, 261, 261, 260, 259,
258, 257, 256, 255, 254, 254, 253, 252, 251,
250, 249, 248, 247, 246, 246, 245, 244, 243,
242, 241, 240, 239, 239, 238, 237, 236, 235,
234, 233, 233, 232, 231, 230, 229, 228, 227,
226, 226, 225, 224, 223, 222, 221, 220, 219,
219, 218, 217, 216, 215, 214, 213, 213, 212,
211, 210, 209, 208, 207, 207, 206, 205, 204,
203, 202, 201, 201, 200, 199, 198, 197, 196,
195, 194, 194, 193, 192, 191, 190, 189, 188,
188, 187, 186, 185, 184, 183, 182, 182, 181,
180, 179, 178, 177, 176, 176, 175, 174, 173,
172, 171, 170, 170, 169, 168, 167, 166, 165,
164, 164, 163, 162, 161, 160, 159, 158, 157,
157, 156, 155, 154, 153, 152, 151, 151, 150,
149, 148, 147, 146, 145, 145, 144, 143, 142,
141, 140, 139, 138, 138, 137, 136, 135, 134,
133, 132, 131, 131, 130, 129, 128, 127, 126,
125, 124, 124, 123, 122, 121, 120, 119, 118,
117, 117, 116, 115, 114, 113, 112, 111, 110,
109, 109, 108, 107, 106, 105, 104, 103, 102,
101, 101, 100, 99, 98, 97, 96, 95, 94, 93,
92, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83,
83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73,
72, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63,
62, 61, 60, 59, 58, 58, 57, 56, 55, 54, 53,
52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42,
41, 40, 39, 38, 37, 36, 36, 35, 34, 33, 32,
31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21,
20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10,
9, 8, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4,
-5, -6, -7, -8, -9, -10, -11, -12, -14, -15,
-16, -17, -18, -19, -20, -21, -22, -23, -24,
-26, -27, -28, -29, -30, -31, -32, -33, -34,
-36, -37, -38, -39, -40, -41, -42, -44, -45,
-46, -47, -48, -49, -51, -52, -53, -54, -55,
-57, -58, -59, -60, -61, -63, -64, -65, -66,
-68, -69, -70, -71, -73, -74, -75, -76, -78,
-79, -80, -81, -83, -84, -85, -87, -88, -89,
-91, -92, -93, -95, -96, -97, -99, -100,
-101, -103, -104, -106, -107, -108, -110,
-111, -113, -114, -116, -117, -118, -120,
-121, -123, -124, -126, -127, -129, -130,
-132, -133, -135, -136, -138, -140, -141,
-143, -144, -146, -148, -149, -151, -152,
-154, -156, -157, -159, -161, -163, -164,
-166, -168, -170, -171, -173, -175, -177,
-178, -180, -182, -184, -186, -188, -190,
-192, -194, -196, -198, -199, -201, -204,
-206, -208, -210, -212, -214, -216, -218,
-220, -223, -225, -227, -229, -232, -234,
-236, -239, -241, -243, -246, -248, -251,
-253, -256, -259, -261, -264, -267, -269,
-272, -275, -278, -281, -284, -287, -290,
-293, -296, -299, -302, -306, -309, -312,
-316, -319, -323, -327, -330, -334, -338,
-342, -346, -351, -355, -359, -364, -368,
-373, -378, -383, -388, -394, -399, -405,
-411, -417, -424, -430, -437, -445, -452,
-460, -469, -478, -487, -498, -509, -521,
-534, -548, -564, -583, -604, -629, -661,
-704, -774, -844
};

// void calculateTemperatureLUT(int32_t * T, int32_t adc_out, int32_t adc_in);

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


// void calculateTemperatureLUT(int32_t * T, int32_t adc_out, int32_t adc_in){
  /* calculate temperature with NTC and pull-up
   * result is T in Q-notation, divide by 2^Q to get the actual value
   *
   * adc_in = ADC value of input voltage
   * adc_out = ADC value of resistor divider voltage
   *
   * calculation used:
   * R_ntc = R_pullup * (V_o/(V_i-V_o))
   */
  


//   int32_t R, result;
  
//   if((adc_out < 0.93*adc_in)&&(adc_out > 0.039*adc_in)){ // sensor has valid range (between -20 deg C and +120 deg C)
    
//     R = q_mul((int32_t) (NTC_R_PULLUP_INT << Q), q_div(adc_out, adc_in - adc_out)) >> Q;  
//     result=interp(NTC_LUT, R, NTC_LUT_SIZE);

//     // if(result > (*T + 100)){ // detect re-connection of temperature sensor, initialize
//     //   *T = result;
//     // }else{
//     //   *T = (1 - TEMPERATURE_FILTER_COEFF) * (*T) + TEMPERATURE_FILTER_COEFF * result;
//     // }
//     *T = result;
//   } else {
//     *T = -1000 << Q; // sensor has invalid reading
//   }
// }

int NTC_ADC2Temperature(unsigned int adc_value){
 
  int p1,p2;
  /* Estimate the interpolating point before and after the ADC value. */
  p1 = NTC_table[ (adc_value >> 2)  ];
  p2 = NTC_table[ (adc_value >> 2)+1];
 
  /* Interpolate between both points. */
  return p1 - ( (p1-p2) * (adc_value & 0x0003) ) / 4;
};

// void calculateTemperatureFloat(float* T, float v_out, float v_in){
//   // calculate temperature with 10k NTC with 10k pull-up from measured voltages v_out and v_in of resistor divider
  
//   // R_ntc = R_pullup * (V_o/(V_i-V_o))
//   // T_ntc = 1/((log(R_ntc/R_ntc_nominal)/NTC_BETA)+1/(25.0+273.15))-273.15
  


//   float R, steinhart;
  
//   if((v_out < 0.91*v_in)&&(v_out > 0.08*v_in)){ // sensor has valid range (between -20 deg C and +90 deg C)
    
//     R = NTC_R_PULLUP * (v_out / (v_in - v_out));
    
//     steinhart = R / NTC_R_NOMINAL;
//     steinhart = log(steinhart);
//     steinhart /= NTC_BETA;
//     steinhart += 1.0 / (25.0 + 273.15);
//     steinhart = 1.0 / steinhart;
//     steinhart -= 273.15;

//     // if(steinhart > (*T + 100)){ // detect re-connection of temperature sensor, initialize
//     //   *T = steinhart;
//     // }else{
//     //   *T = (1 - TEMPERATURE_FILTER_COEFF) * (*T) + TEMPERATURE_FILTER_COEFF * steinhart;
//     // }

//       // ESP_LOGI(TEMP_TAG, "T %f", T);
//     *T = steinhart;
    
//   } else {
//     *T = -1e3; // sensor has invalid reading
//   }
// }