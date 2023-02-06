#include "logger.h"
#include <stdio.h>
#include "common.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"

#include "fileman.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_sd_card.h"
#include "hw_config.h"
#include "settings.h"
#include "spi_control.h"
#include "tempsensor.h"
#include "time.h"



uint8_t * spi_buffer;
spi_msg_1_t * spi_msg_1_ptr;
spi_msg_2_t * spi_msg_2_ptr;

uint8_t msg_part = 0;

struct {
    uint8_t timeData[TIME_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
    uint8_t adcData[ADC_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
    uint8_t gpioData[GPIO_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
}sdcard_data;


// Same size as SPI buffer but then int32 size. 
int32_t adc_buffer_fixed_point[(ADC_VALUES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH)];

char strbuffer[16];

LoggerState_t _currentLogTaskState = LOGTASK_IDLE;
LoggerState_t _nextLogTaskState = LOGTASK_IDLE;
LoggingState_t _currentLoggingState = LOGGING_IDLE;
LoggingState_t _nextLoggingState = LOGGING_IDLE;

// temporary variables to calculate fixed point numbers
int64_t t0, t1,t2,t3;




// Handle to stm32 task
extern TaskHandle_t xHandle_stm32;

// State of STM interrupt pin. 0 = low, 1 = high
// bool int_level = 0;
// Interrupt counter that tracks how many times the interrupt has been triggered. 
uint8_t volatile int_counter =0;

static uint8_t log_counter = 0;

int32_t Logger_convertAdcFixedPoint(uint8_t adcData0, uint8_t adcData1, uint64_t range, uint64_t offset)
{
    t0 = ((int32_t)adcData0 | ((int32_t)adcData1 << 8));
            
    // In one buffer of STM_TXLENGTH bytes, there are only STM_TXLENGTH/2 16 bit ADC values. So divide by 2
    t1 = t0 * (-1LL* range); // note the minus for inverted input!
    t2 = t1 / ((1 << settings_get_resolution()) - 1); // -1 for 4095 steps
    t3 = t2 + offset;
    return (int32_t) t3;
    
}

float Logger_convertAdcFloat(uint16_t adcData0, uint16_t adcData1)
{
    float t0_f, t1_f, t2_f, t3_f;
    t0_f = (adcData0 | (adcData1 << 8));
            
    // In one buffer of STM_TXLENGTH bytes, there are only STM_TXLENGTH/2 16 bit ADC values. So divide by 2

    t1_f = t0_f * (-20.0); // note the minus for inverted input!
    t2_f = t1_f / ((1 << settings_get_resolution()) - 1); // -1 for 4095 steps
    t3_f = t2_f + 10.0;
    return t3_f;
}



LoggerState_t LogTaskGetState()
{
    return _currentLogTaskState;
}

uint8_t Logger_enterSettingsMode()
{
    // Typical usage, go to settings mode, set settings, sync settings, exit settings mode
    if (_currentLogTaskState == LOGTASK_IDLE)
    {
        _nextLogTaskState = LOGTASK_SETTINGS;
        return RET_OK;
    } else {
        return RET_NOK;
    }
}

uint8_t Logger_exitSettingsMode()
{
     // Typical usage, go to settings mode, set settings, sync settings, exit settings mode
    if (_currentLogTaskState == LOGTASK_SETTINGS)
    {
        if (Logger_syncSettings() == RET_OK)
        {
            _nextLogTaskState = LOGTASK_IDLE;
            return RET_OK;
        } else {
            return RET_NOK;
        }
    } else {
        return RET_NOK;
    }
}

uint8_t Logger_isLogging(void)
{
    if (_currentLogTaskState == LOGTASK_LOGGING)
    {
        return RET_OK;
    } else {
        return RET_NOK;
    }
}




void Logger_GetSingleConversion(converted_reading_t * dataOutput)
{
    int j =0;
    struct tm t = {0};
    float tfloat = 0.0;

    uint16_t adc0, adc1; 
    

    for (int i =0; i < 16; i=i+2)
    {
        if (!msg_part)
        {
            adc0 = spi_msg_1_ptr->adcData[i];
            adc1 = spi_msg_1_ptr->adcData[i+1];
        } else {
            adc0 = spi_msg_2_ptr->adcData[i];
            adc1 = spi_msg_2_ptr->adcData[i+1];
        }

        dataOutput->analogData[j] = Logger_convertAdcFloat(adc0,  adc1);
        calculateTemperatureFloat(&tfloat, (float)(adc0 | (adc1 << 8)) , (float)(0x01 << settings_get_resolution())-1);
        
        dataOutput->temperatureData[j] = tfloat;
        j++;
    }

    if (!msg_part)
    {
        dataOutput->gpioData[0] = (spi_msg_1_ptr->gpioData[0] & 0x04) && 1;
        dataOutput->gpioData[1] = (spi_msg_1_ptr->gpioData[0] & 0x08) && 1;
        dataOutput->gpioData[2] = (spi_msg_1_ptr->gpioData[0] & 0x10) && 1;
        dataOutput->gpioData[3] = (spi_msg_1_ptr->gpioData[0] & 0x20) && 1;
        dataOutput->gpioData[4] = (spi_msg_1_ptr->gpioData[0] & 0x40) && 1;
        dataOutput->gpioData[5] = (spi_msg_1_ptr->gpioData[0] & 0x80) && 1;
    } else {
        dataOutput->gpioData[0] = (spi_msg_2_ptr->gpioData[0] & 0x04) && 1;
        dataOutput->gpioData[1] = (spi_msg_2_ptr->gpioData[0] & 0x08) && 1;
        dataOutput->gpioData[2] = (spi_msg_2_ptr->gpioData[0] & 0x10) && 1;
        dataOutput->gpioData[3] = (spi_msg_2_ptr->gpioData[0] & 0x20) && 1;
        dataOutput->gpioData[4] = (spi_msg_2_ptr->gpioData[0] & 0x40) && 1;
        dataOutput->gpioData[5] = (spi_msg_2_ptr->gpioData[0] & 0x80) && 1;
    }
    

    if (!msg_part)
    {
        t.tm_hour = spi_msg_1_ptr->timeData->hours;
        t.tm_min = spi_msg_1_ptr->timeData->minutes;
        t.tm_sec = spi_msg_1_ptr->timeData->seconds;
        t.tm_year = spi_msg_1_ptr->timeData->year+100;
        t.tm_mon = spi_msg_1_ptr->timeData->month;
        t.tm_mday = spi_msg_1_ptr->timeData->date;
    } else {
        t.tm_hour = spi_msg_2_ptr->timeData->hours;
        t.tm_min = spi_msg_2_ptr->timeData->minutes;
        t.tm_sec = spi_msg_2_ptr->timeData->seconds;
        t.tm_year = spi_msg_2_ptr->timeData->year+100;
        t.tm_mon = spi_msg_2_ptr->timeData->month;
        t.tm_mday = spi_msg_2_ptr->timeData->date;
    }
    

    dataOutput->timestamp  = (uint32_t)mktime(&t);    
}

esp_err_t Logger_singleShot()
{
    converted_reading_t measurement;
    uint8_t * spi_buffer = spi_ctrl_getRxData();

    if (Logger_getState() != LOGTASK_LOGGING)
    {
        if (spi_ctrl_cmd(STM32_CMD_SINGLE_SHOT_MEASUREMENT, 0) == ESP_OK)
        {
            
            if (!((spi_buffer[0] == STM32_CMD_SINGLE_SHOT_MEASUREMENT) && (spi_buffer[1] == STM32_RESP_OK)))
            {
                spi_ctrl_print_rx_buffer(spi_buffer);
                ESP_LOGE(TAG_LOG, "Did not receive STM confirmation.");
                return ESP_FAIL;
            } 
            
        } else {
            ESP_LOGE(TAG_LOG, "Single shot command failed.");
            return ESP_FAIL;
        }
        
        msg_part = 0;
        
        // Get the data from the STM32
        return (spi_ctrl_receive_data(sizeof(spi_msg_1_t) == ESP_OK));        
    } 
    else if (Logger_getState() == LOGTASK_LOGGING)
    {
        // Data should be already available, since normal logging is enabled
        return ESP_OK;   
    } else {
        return ESP_FAIL;
    }
    
    
}

uint8_t Logger_syncSettings()
{
    settings_persist_settings();
    // Send command to STM32 to go into settings mode
    ESP_LOGI(TAG_LOG, "Setting SETTINGS mode");
    spi_buffer = spi_ctrl_getRxData();


    if (spi_ctrl_datardy_int(0) != ESP_OK)
        return ESP_FAIL;


    
    

    if (spi_ctrl_cmd(STM32_CMD_SETTINGS_MODE, 0) == ESP_OK)
    {
        // spi_ctrl_print_rx_buffer();
        if (spi_buffer[0] != STM32_CMD_SETTINGS_MODE || spi_buffer[1] != STM32_RESP_OK)
        {
            ESP_LOGI(TAG_LOG, "Unable to put STM32 into SETTINGS mode. ");
            spi_ctrl_print_rx_buffer(spi_buffer);
            return RET_NOK;
        } 
        ESP_LOGI(TAG_LOG, "SETTINGS mode enabled");
    } else {
        return RET_NOK;
    }

    Settings_t * settings = settings_get();

     spi_ctrl_cmd(STM32_CMD_SET_ADC_CHANNELS_ENABLED, settings_get_adc_channel_enabled_all());
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_ADC_CHANNELS_ENABLED || spi_buffer[1] != STM32_RESP_OK)
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 ADC channels. Received %d", spi_buffer[0]);
        spi_ctrl_print_rx_buffer(spi_buffer);
        return RET_NOK;
    }

    ESP_LOGI(TAG_LOG, "ADC channels set");

    spi_ctrl_cmd(STM32_CMD_SET_RESOLUTION, (uint8_t)settings_get_resolution());
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_RESOLUTION || spi_buffer[1] != STM32_RESP_OK)
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 ADC resolution. Received %d", spi_buffer[0]);
        spi_ctrl_print_rx_buffer(spi_buffer);
        return RET_NOK;
    } 
    
    ESP_LOGI(TAG_LOG, "ADC resolution set");
    

    spi_ctrl_cmd(STM32_CMD_SET_SAMPLE_RATE, (uint8_t)settings_get_samplerate());
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_SAMPLE_RATE || spi_buffer[1] != STM32_RESP_OK )
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 sample rate. ");
        spi_ctrl_print_rx_buffer(spi_buffer);
        return RET_NOK;
    }

    ESP_LOGI(TAG_LOG, "Sample rate set");

    // Send settings one by one and confirm
    spi_ctrl_cmd(STM32_CMD_MEASURE_MODE, 0);
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_MEASURE_MODE || spi_buffer[1] != STM32_RESP_OK )
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 in measure mode");
        spi_ctrl_print_rx_buffer(spi_buffer);
        return RET_NOK;
    }

    // Re-enable interrupts
    if (spi_ctrl_datardy_int(1) != ESP_OK)
        return ESP_FAIL;

    
    ESP_LOGI(TAG_LOG, "Sync done");
    // Exit settings mode 
    return RET_OK;
}

uint8_t Logger_setCsvLog(log_mode_t value)
{
    if (value == LOGMODE_CSV || value == LOGMODE_RAW)
    {
        return settings_set_logmode(value);
    } else{
        return RET_NOK;
    }
    
}

uint8_t Logger_getCsvLog()
{
    return settings_get_logmode();
}

uint8_t Logger_mode_button_pushed()
{
    static uint8_t state = 0;
    uint8_t level = gpio_get_level(GPIO_START_STOP_BUTTON);
    
    switch (state)
    {
        case 0:
          if(!level) 
          {
            // ESP_LOGI(TAG_LOG, "MODE press hold");
            state = 1;
          }
        break;
            
        case 1:
            if(level)
            {
                // ESP_LOGI(TAG_LOG, "MODE released");
                state = 2;
                return 1;
            } 
        break;

        case 2:
            // ESP_LOGI(TAG_LOG, "MODE reset");
            state = 0;
            
        break;    
    }
    
    return 0;
}

esp_err_t Logger_start()
{
    if (_currentLogTaskState == LOGTASK_IDLE || _currentLogTaskState == LOGTASK_ERROR_OCCURED)
    {
        // gpio_set_level(GPIO_ADC_EN, 1);
        _nextLogTaskState = LOGTASK_LOGGING;
        return ESP_OK;
    } 
    else 
    {
        return ESP_FAIL;
    }
}

esp_err_t Logger_stop()
{
     ESP_LOGI(TAG_LOG, "Logger_stop() called");
     
    if (_currentLogTaskState == LOGTASK_LOGGING)
    {
        // Disable interrupt data ready pin
        _nextLogTaskState = LOGTASK_STOPPING;
       
        return ESP_OK;
    } 
    else 
    {
        return ESP_FAIL;
    }
}

size_t Logger_flush_buffer_to_sd_card_uint8(uint8_t * buffer, size_t size)
{
    ESP_LOGI(TAG_LOG, "Flusing buffer to SD card");
    
    return fileman_write(buffer, size);
    
   
}

size_t Logger_flush_buffer_to_sd_card_csv(int32_t * adcData, size_t lenAdc, uint8_t * gpioData, size_t lenGpio, uint8_t * timeData, size_t lenTime)
{
    ESP_LOGI(TAG_LOG, "Flusing CSV buffer to SD card");
    return fileman_csv_write(adcData, lenAdc, gpioData, lenGpio, timeData ,lenTime);
}

// uint8_t Logger_raw_to_csv(uint8_t * buffer, size_t size, uint8_t log_counter)
uint8_t Logger_raw_to_csv(uint8_t log_counter, const uint8_t * adcData, size_t length, uint8_t range, uint8_t type)
{
     
        int j,x=0;
        uint32_t writeptr = 0;
        uint64_t channel_range, channel_offset;
        uint16_t adc0, adc1=0;
        ESP_LOGI(TAG_LOG, "raw_to_csv log_counter %d", log_counter);
        for (j = 0; j < length; j = j + 2)
        {
            // Check for each channel what the range is (each bit in 'range' is 0 (=-10/+10V range) or 1 (=-60/+60V range) )
            if ((range >> x) & 0x01)
            {
                // Bit == 1
                channel_range = 120000000LL;
                channel_offset = 60000000LL;
            } else {
                channel_range = 20000000LL;
                channel_offset = 10000000LL;
            }

            // Detect type of sensor and conver accordingly
            if ((type >> x) & 0x01)
            {
                // ESP_LOGI(TAG_LOG,"temp detected");
                // calculateTemperatureLUT(&(adc_buffer_fixed_point[writeptr+(log_counter*(length/2))]), ((uint16_t)adcData[j]) | ((uint16_t)adcData[j+1] << 8), (1 << settings_get_resolution()) - 1);
                adc_buffer_fixed_point[writeptr+(log_counter*(length/2))] = NTC_ADC2Temperature((uint16_t)adcData[j] | ((uint16_t)adcData[j+1] << 8))*100000;
            } else {
                adc_buffer_fixed_point[writeptr+(log_counter*(length/2))] = Logger_convertAdcFixedPoint(adcData[j], adcData[j+1], channel_range, channel_offset);    
            }
            

            x++;
            x = x % 8;
            writeptr++;
        }

        // j = j - 2;
    // ESP_LOGI(TAG_LOG, "%d, %d, %d, %d, %lld, %d", writeptr, j, recvbuf0.adc[j], recvbuf0.adc[j+1], t3, tbuffer_i32[writeptr+(log_counter)*(960/2)]);
        return RET_OK;
}

LoggerState_t Logger_getState()
{
    return _currentLogTaskState;
}

esp_err_t Logger_flush_to_sdcard()
{
     // Flush buffer to sd card
    if (settings_get_logmode() == LOGMODE_CSV)
    {
        ESP_LOGI(TAG_LOG, "ADC fixed p %d, %d, %d", adc_buffer_fixed_point[0], adc_buffer_fixed_point[1], adc_buffer_fixed_point[2]);
                    ESP_LOGI(TAG_LOG, "GPIO %d, %d, %d", sdcard_data.gpioData[0], sdcard_data.gpioData[1], sdcard_data.gpioData[2]);
                    ESP_LOGI(TAG_LOG, "Sizes: %d, %d, %d", sizeof(adc_buffer_fixed_point), sizeof(sdcard_data.gpioData), sizeof(sdcard_data.timeData));
                    
            if (!Logger_flush_buffer_to_sd_card_csv(
                        adc_buffer_fixed_point, (sizeof(adc_buffer_fixed_point)/sizeof(int32_t)),
                        sdcard_data.gpioData, sizeof(sdcard_data.gpioData), 
                        sdcard_data.timeData, (sizeof(sdcard_data.timeData)/sizeof(s_date_time_t))) )
                        {
                            return ESP_FAIL;
                        }
    } else {
        if (Logger_flush_buffer_to_sd_card_uint8((uint8_t*)&sdcard_data, SD_BUFFERSIZE) != SD_BUFFERSIZE)
        {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
   
}

esp_err_t Logger_log()
{   
    
  
    
    static uint8_t count_offset = 1;

    
  

        switch (_currentLoggingState)
        {
            
            case LOGGING_START:
                    
                    if (int_counter==0)
                    {
                        count_offset = 0;
                    } 
                    spi_ctrl_queue_msg(NULL, sizeof(spi_msg_1_t));
                    _nextLoggingState = LOGGING_RX0_WAIT;
                    
                       
            break;
            
            case LOGGING_RX0_WAIT:
                {

                    
                        if (spi_ctrl_receive_data() == ESP_OK)
                        {
                            
                            // there is data
                            if (spi_msg_1_ptr->startByte[0] == 0xFF &&
                                spi_msg_1_ptr->startByte[1] == 0xFF)
                                {
                                    msg_part = 0;
                                    ESP_LOGI(TAG_LOG, "Start bytes found 1/2");
                                    // In this case we have Time bytes first...
                                    memcpy(sdcard_data.timeData+log_counter*sizeof(spi_msg_1_ptr->timeData), 
                                        spi_msg_1_ptr->timeData, 
                                        sizeof(spi_msg_1_ptr->timeData));
                                    // Then GPIO...
                                    memcpy(sdcard_data.gpioData+log_counter*sizeof(spi_msg_1_ptr->gpioData), 
                                        spi_msg_1_ptr->gpioData, 
                                        sizeof(spi_msg_1_ptr->gpioData)); 
                                    // And finally ADC bytes
                                    memcpy(sdcard_data.adcData+log_counter*sizeof(spi_msg_1_ptr->adcData), 
                                        spi_msg_1_ptr->adcData,
                                        sizeof(spi_msg_1_ptr->adcData));

                                    ESP_LOGI(TAG_LOG, "Time: %d, %d, %d", sdcard_data.timeData[log_counter*sizeof(spi_msg_1_ptr->timeData)], sdcard_data.timeData[log_counter*sizeof(spi_msg_1_ptr->timeData)+1], sdcard_data.timeData[log_counter*sizeof(spi_msg_1_ptr->timeData)+2]);
                                    ESP_LOGI(TAG_LOG, "GPIO: %d, %d, %d", sdcard_data.gpioData[log_counter*sizeof(spi_msg_1_ptr->gpioData)], sdcard_data.gpioData[log_counter*sizeof(spi_msg_1_ptr->gpioData)+1], sdcard_data.gpioData[log_counter*sizeof(spi_msg_1_ptr->gpioData)+2]);
                                    ESP_LOGI(TAG_LOG, "ADC: %d, %d, %d, %d", sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)], sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)+1], sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)+2], sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)+3]);
                                    ESP_LOGI(TAG_LOG, "dataLen: %d", spi_msg_1_ptr->dataLen);
                                } else if (spi_msg_2_ptr->stopByte[0] == 0xFF &&
                                    spi_msg_2_ptr->stopByte[1] == 0xFF )
                                {
                                    msg_part = 1;
                                    // Now the order is reversed.
                                    ESP_LOGI(TAG_LOG, "Start bytes found 2/2");
                                    // First ADC bytes..
                                    memcpy(sdcard_data.adcData+log_counter*sizeof(spi_msg_2_ptr->adcData), 
                                        spi_msg_2_ptr->adcData,
                                        sizeof(spi_msg_2_ptr->adcData));
                                    
                                    // Then GPIO bytes    
                                    memcpy(sdcard_data.gpioData+log_counter*sizeof(spi_msg_2_ptr->gpioData), 
                                        spi_msg_2_ptr->gpioData, 
                                        sizeof(spi_msg_2_ptr->gpioData)); 
                                    
                                    // Finally Time bytes
                                    memcpy(sdcard_data.timeData+log_counter*sizeof(spi_msg_2_ptr->timeData), 
                                        spi_msg_2_ptr->timeData, 
                                        sizeof(spi_msg_2_ptr->timeData)); 
                                    ESP_LOGI(TAG_LOG, "Time: %d, %d, %d", sdcard_data.timeData[log_counter*sizeof(spi_msg_2_ptr->timeData)], sdcard_data.timeData[log_counter*sizeof(spi_msg_2_ptr->timeData)+1], sdcard_data.timeData[log_counter*sizeof(spi_msg_2_ptr->timeData)+2]);
                                    ESP_LOGI(TAG_LOG, "GPIO: %d, %d, %d", sdcard_data.gpioData[log_counter*sizeof(spi_msg_2_ptr->gpioData)], sdcard_data.gpioData[log_counter*sizeof(spi_msg_2_ptr->gpioData)+1], sdcard_data.gpioData[log_counter*sizeof(spi_msg_2_ptr->gpioData)+2]);
                                    ESP_LOGI(TAG_LOG, "ADC: %d, %d, %d, %d", sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)], sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)+1], sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)+2], sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)+3]);
                                    ESP_LOGI(TAG_LOG, "dataLen: %d", spi_msg_2_ptr->dataLen);
                                    // }
                                }  else {
                                    //     // No start or stop byte found!
                                    ESP_LOGE(TAG_LOG, "No start or stop byte found! Stop bytes: %d, %d and %d, %d", spi_msg_1_ptr->startByte[0], spi_msg_1_ptr->startByte[1], spi_msg_2_ptr->stopByte[0], spi_msg_2_ptr->stopByte[1]);
                                    return ESP_FAIL;
                                }
                            
                                // Process data first
                                if (settings_get_logmode() == LOGMODE_CSV)
                                {
                                    Logger_raw_to_csv(log_counter, sdcard_data.adcData+log_counter*sizeof(spi_msg_1_ptr->adcData), sizeof(spi_msg_1_ptr->adcData), 
                                    settings_get_adc_channel_range_all(), 
                                    settings_get_adc_channel_type_all());
                                }
                                
                                // Logger_flush_buffer_to_sd_card_csv(adc_buffer_fixed_point, 8, sdcard_data.gpioData, 1, sdcard_data.timeData, 1);
                                // for (int i=0; i<sizeof(adc_buffer_fixed_point)/16; i++)
                                // {
                                //     // ESP_LOGI(TAG_LOG, "ADC %d: %d", i, sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)+i]);
                                //     ESP_LOGI(TAG_LOG, "ADC %d: %d", i, adc_buffer_fixed_point[log_counter*sizeof(adc_buffer_fixed_point)/16+i]);
                                // }
                                
                                log_counter++; // received bytes = log_counter*512
                                
                                // Check, for example, gpio data size to keep track if sdcard_data is full
                                // Change in the future
                                
                            // Set RX state to NODATA
                            spi_ctrl_reset_rx_state();
                            _nextLoggingState = LOGGING_IDLE;
                        }
                           
                            
                
                }

                    
                            // ESP_LOGI(TAG_LOG, "log_counter:%d", log_counter);
                            // // for (int i=0; i<sizeof(recvbuf0)/4; i++)
                            // // {
                            // //     ESP_LOGI(TAG_LOG, "Byte %d, %d", i, (recvbuf0[i]));
                            // // }

                            // //  for (int i=sizeof(recvbuf0)*3/4; i<sizeof(recvbuf0); i++)
                            // // {
                            // //     ESP_LOGI(TAG_LOG, "Byte %d, %d", i, (recvbuf0[i]));
                            // // }
                            
                            // if (log_counter != (int_counter-1))
                            // {

                            //     ESP_LOGE(TAG_LOG, "Missing SPI transaction (%d vs. %d)! Stopping", log_counter, (int_counter));
                            //     return ESP_FAIL;
                            // }


                            // Copy values to buffer

                            // Check if the start bytes are first or last in the received SPI buffer
                           
                            
                                // if (settings_get_logmode() == LOGMODE_CSV)
                                // {
                                //     // Write it SD
                                //     ESP_LOGI(TAG_LOG, "ADC fixed p %d, %d, %d", adc_buffer_fixed_point[0], adc_buffer_fixed_point[1], adc_buffer_fixed_point[2]);
                                //     ESP_LOGI(TAG_LOG, "GPIO %d, %d, %d", sdcard_data.gpioData[0], sdcard_data.gpioData[1], sdcard_data.gpioData[2]);
                                //     ESP_LOGI(TAG_LOG, "Sizes: %d, %d, %d", sizeof(adc_buffer_fixed_point), sizeof(sdcard_data.gpioData), sizeof(sdcard_data.timeData));
                                    
                                    
                                //     Logger_flush_buffer_to_sd_card_csv(
                                //         adc_buffer_fixed_point, (sizeof(adc_buffer_fixed_point)/sizeof(int32_t)),
                                //         sdcard_data.gpioData, sizeof(sdcard_data.gpioData), 
                                //         sdcard_data.timeData, (sizeof(sdcard_data.timeData)/sizeof(s_date_time_t)) );
                                // } else {
                                //     Logger_flush_buffer_to_sd_card_uint8((uint8_t*)&sdcard_data, sizeof(sdcard_data));
                                // }                    
                                
                            // }

                       
            //             } else {
            //                 ESP_LOGE(TAG_LOG, "RX0 timed out!");
            //                 _nextLoggingState = LOGGING_IDLE;
            //                 return ESP_ERR_TIMEOUT;
            //             }
            //             _nextLoggingState = LOGGING_IDLE;
            //     }
            //     break;


            case LOGGING_IDLE:
            {
                rxdata_state_t state;
                state = spi_ctrl_rxstate();

                if (state == RXDATA_STATE_DATA_READY)
                {
                    _nextLoggingState = LOGGING_START;
                }
                 
                if (state == RXDATA_STATE_DATA_OVERRUN) 
                {
                    ESP_LOGE(TAG_LOG, "Error receiving data from STM32");
                    return ESP_FAIL;
                }
                
            }
            break;
        }

       
       

        if (_nextLoggingState != _currentLoggingState)
        {
            
            ESP_LOGI(TAG_LOG, "LOGGING state changing from %d to %d", _currentLoggingState, _nextLoggingState);
            _currentLoggingState = _nextLoggingState;
        }
                
    


    return ESP_OK;
    
    
}


void task_logging(void * pvParameters)
{
    

    esp_err_t ret;
   
    // Init STM32 ADC enable pin
    // gpio_set_direction(GPIO_DATA_RDY_PIN, GPIO_MODE_INPUT);


    gpio_set_direction(GPIO_START_STOP_BUTTON, GPIO_MODE_INPUT);
    
    gpio_config_t adc_en_conf={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1<<GPIO_ADC_EN)
    };

    ret = gpio_config(&adc_en_conf);
    gpio_set_level(GPIO_ADC_EN, 0);

    
    // // Initialize SD card
    if (esp_sd_card_mount() == ESP_OK)
    {
        ESP_LOGI(TAG_LOG, "File seq nr: %d", fileman_search_last_sequence_file());
        esp_sd_card_unmount();
    } 
    
    ESP_LOGI(TAG_LOG, "Logger task started");


    if (spi_ctrl_init(STM32_SPI_HOST, GPIO_DATA_RDY_PIN) != ESP_OK)
    {
        // throw error
        ESP_LOGE(TAG_LOG, "Unable to initialize the SPI data controller!");
        while(1);
    }

    if (Logger_syncSettings() )
    {
        ESP_LOGE(TAG_LOG, "STM32 settings FAILED");
    } else {
        ESP_LOGI(TAG_LOG, "STM32 settings synced");
    }

    
    spi_msg_1_ptr = (spi_msg_1_t*)spi_buffer;
    spi_msg_2_ptr = (spi_msg_2_t*)spi_buffer;


    while(1) {
       
        if (Logger_mode_button_pushed())
        {
            if (_currentLogTaskState == LOGTASK_IDLE || _currentLogTaskState == LOGTASK_ERROR_OCCURED)
            {
                Logger_start();
            }
            
            if (_currentLogTaskState == LOGTASK_LOGGING)
            {
                Logger_stop();
            }
        }

        spi_ctrl_loop();

        switch (_currentLogTaskState)
        {
            case LOGTASK_IDLE:
            case LOGTASK_ERROR_OCCURED:
                // Logger_singleShot();
                vTaskDelay(500 / portTICK_PERIOD_MS);
                
                if (_nextLogTaskState == LOGTASK_LOGGING)
                {
                    if (esp_sd_card_mount() == ESP_OK)
                    {
                        ESP_LOGI(TAG_LOG, "File seq nr: %d", fileman_search_last_sequence_file());
                        if (fileman_open_file() != ESP_OK)
                        { 
                            if (esp_sd_card_unmount() == ESP_OK)
                            {
                                _nextLogTaskState = LOGTASK_ERROR_OCCURED;
                            }

                            break;
                        }
                        fileman_csv_write_header();
                        // _nextLoggingState = LOGGING_START;
                        // // upon changing state to logging, make sure these settings are correct. 
                        // // _spi_transaction_rx0.length=sizeof(sendbuf)*8; // size in bits
                        // _spi_transaction_rx0.length=STM_SPI_BUFFERSIZE_DATA_RX*8; // size in bits
                        // _spi_transaction_rx0.rxlength = STM_SPI_BUFFERSIZE_DATA_RX*8; // size in bits
                        // // _spi_transaction.tx_buffer=sendbuf;
                        // _spi_transaction_rx0.rx_buffer=(uint8_t*)&recvbuf0;
                        // _spi_transaction_rx0.tx_buffer=NULL;

                        // _nextLoggingState = LOGGING_IDLE;
                        // esp_sd_card_mount_open_file();
                        // enable data_rdy interrupt
                        // assert(Logger_datardy_int(1) == RET_OK);
                        // Enable logging
                        gpio_set_level(GPIO_ADC_EN, 1);
                    } else {
                          _nextLogTaskState = LOGTASK_ERROR_OCCURED;
                    }
                   
                   
                }
            break;

            case LOGTASK_LOGGING:
                ret = Logger_log();
                if (ret == ESP_FAIL )
                {
                    ESP_LOGI(TAG_LOG, "Error occured in Logging statemachine. Stopping..");
                    _nextLogTaskState = LOGTASK_ERROR_OCCURED;
                }


                if ( _nextLogTaskState == LOGTASK_ERROR_OCCURED || _nextLogTaskState == LOGTASK_STOPPING)
                {
                    gpio_set_level(GPIO_ADC_EN, 0); 
                    // disable data_rdy interrupt
                    // if (_nextLogTaskState == LOGTASK_ERROR_OCCURED)
                    // {
                        // Logger_datardy_int(0);
                    // }

                }

                if(log_counter*sizeof(spi_msg_1_ptr->gpioData) >= sizeof(sdcard_data.gpioData))
                {
                    Logger_flush_to_sdcard();
                    log_counter = 0;
                    int_counter = 0;
                }


            break;

            case LOGTASK_STOPPING:
                ret = Logger_log();
                
                if ( ret == ESP_OK )
                {
                    ESP_LOGI(TAG_LOG, "Last msg received");
                     
                //   disable data_rdy interrupt
                    // if (Logger_datardy_int(0) != ESP_OK)
                    // {
                    //     _nextLogTaskState = LOGTASK_ERROR_OCCURED;
                    // }

                    ESP_LOGI(TAG_LOG, "Flusing buffer");
                    Logger_flush_to_sdcard();
                    fileman_close_file();
                    esp_sd_card_unmount();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    _nextLogTaskState = LOGTASK_IDLE;
                // } else {
                //     ESP_LOGI(TAG_LOG, "Waiting for last message...");
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                  

            break;

            case LOGTASK_SETTINGS:

            
            break;


            default:

            break;
            // should not come here
        }

        if (_nextLogTaskState != _currentLogTaskState)
        {
            ESP_LOGI(TAG_LOG, "Changing LOGTASK state from %d to %d", _currentLogTaskState, _nextLogTaskState);
            _currentLogTaskState = _nextLogTaskState;
        }


    }
     
    
}

