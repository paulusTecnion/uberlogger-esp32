void calculateTemperature(float* T, float v_out, float v_in){
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

    if(steinhart > (*T + 100)){ // detect re-connection of temperature sensor, initialize
      *T = steinhart;
    }else{
      *T = (1 - TEMPERATURE_FILTER_COEFF) * (*T) + TEMPERATURE_FILTER_COEFF * steinhart;
    }
    
  } else {
    *T = -1e3; // sensor has invalid reading
  }
}