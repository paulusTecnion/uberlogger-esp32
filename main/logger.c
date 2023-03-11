#include "logger.h"
#include <stdio.h>
#include "common.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"

#include "errorcodes.h"
#include "fileman.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_sd_card.h"
#include "config.h"
#include "settings.h"
#include "spi_control.h"
#include "tempsensor.h"
#include "time.h"



uint8_t * spi_buffer;
spi_msg_1_t * spi_msg_1_ptr;
spi_msg_2_t * spi_msg_2_ptr;
uint8_t _stopLogging = 0;
uint8_t _startLogging = 0;
uint8_t _startLogTask = 0;
uint8_t _stopLogTask = 0;

uint8_t _dataReceived = 0;
uint64_t stm32TimerTimeout, currtime_us =0;

typedef struct {
    uint8_t startByte[START_STOP_NUM_BYTES]; // 2
    uint16_t dataLen;
    s_date_time_t timeData; //12*70 = 840
    uint8_t padding3;
    uint8_t padding4;
    uint8_t gpioData; // 70
    uint8_t adcData[16]; // 1120
} live_data_t;

live_data_t live_data_buffer;

uint8_t msg_part = 0, expected_msg_part = 0;

struct {
    uint8_t timeData[TIME_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
    uint8_t adcData[ADC_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
    uint8_t gpioData[GPIO_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
    uint32_t datarows;
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
uint32_t _errorCode;

// Handle to stm32 task
extern TaskHandle_t xHandle_stm32;

// State of STM interrupt pin. 0 = low, 1 = high
// bool int_level = 0;
// Interrupt counter that tracks how many times the interrupt has been triggered. 


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

uint32_t LogTaskGetError()
{
    return _errorCode;
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

// uint8_t Logger_exitSettingsMode()
// {
//      // Typical usage, go to settings mode, set settings, sync settings, exit settings mode
//     if (_currentLogTaskState == LOGTASK_SETTINGS)
//     {
//         if (Logger_syncSettings() == ESP_OK)
//         {
//             _nextLogTaskState = LOGTASK_IDLE;
//             return RET_OK;
//         } else {
//             return RET_NOK;
//         }
//     } else {
//         return RET_NOK;
//     }
// }

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
        // if (!msg_part)
        // {
        //     adc0 = spi_msg_1_ptr->adcData[i];
        //     adc1 = spi_msg_1_ptr->adcData[i+1];
        // } else {
        //     adc0 = spi_msg_2_ptr->adcData[i];
        //     adc1 = spi_msg_2_ptr->adcData[i+1];
        // }




        adc0 = live_data_buffer.adcData[i];
        adc1 = live_data_buffer.adcData[i+1];

        dataOutput->analogData[j] = Logger_convertAdcFloat(adc0,  adc1);
        calculateTemperatureFloat(&tfloat, (float)(adc0 | (adc1 << 8)) , (float)(0x01 << settings_get_resolution())-1);
        
        dataOutput->temperatureData[j] = tfloat;
        j++;
    }

    // if (!msg_part)
    // {
        // dataOutput->gpioData[0] = (spi_msg_1_ptr->gpioData[0] & 0x04) && 1;
        // dataOutput->gpioData[1] = (spi_msg_1_ptr->gpioData[0] & 0x08) && 1;
        // dataOutput->gpioData[2] = (spi_msg_1_ptr->gpioData[0] & 0x10) && 1;
        // dataOutput->gpioData[3] = (spi_msg_1_ptr->gpioData[0] & 0x20) && 1;
        // dataOutput->gpioData[4] = (spi_msg_1_ptr->gpioData[0] & 0x40) && 1;
        // dataOutput->gpioData[5] = (spi_msg_1_ptr->gpioData[0] & 0x80) && 1;
        dataOutput->gpioData[0] = (live_data_buffer.gpioData & 0x04) && 1;
        dataOutput->gpioData[1] = (live_data_buffer.gpioData & 0x08) && 1;
        dataOutput->gpioData[2] = (live_data_buffer.gpioData & 0x10) && 1;
        dataOutput->gpioData[3] = (live_data_buffer.gpioData & 0x20) && 1;
        dataOutput->gpioData[4] = (live_data_buffer.gpioData & 0x40) && 1;
        dataOutput->gpioData[5] = (live_data_buffer.gpioData & 0x80) && 1;
    // } else {
    //     dataOutput->gpioData[0] = (spi_msg_2_ptr->gpioData[0] & 0x04) && 1;
    //     dataOutput->gpioData[1] = (spi_msg_2_ptr->gpioData[0] & 0x08) && 1;
    //     dataOutput->gpioData[2] = (spi_msg_2_ptr->gpioData[0] & 0x10) && 1;
    //     dataOutput->gpioData[3] = (spi_msg_2_ptr->gpioData[0] & 0x20) && 1;
    //     dataOutput->gpioData[4] = (spi_msg_2_ptr->gpioData[0] & 0x40) && 1;
    //     dataOutput->gpioData[5] = (spi_msg_2_ptr->gpioData[0] & 0x80) && 1;
    // }
    

    // if (!msg_part)
    // {
        t.tm_hour = live_data_buffer.timeData.hours;
        t.tm_min =  live_data_buffer.timeData.minutes;
        t.tm_sec =  live_data_buffer.timeData.seconds;
        t.tm_year = live_data_buffer.timeData.year+100;
        t.tm_mon =  live_data_buffer.timeData.month-1;
        t.tm_mday = live_data_buffer.timeData.date;
    // } else {
    //     t.tm_hour = spi_msg_2_ptr->timeData->hours;
    //     t.tm_min = spi_msg_2_ptr->timeData->minutes;
    //     t.tm_sec = spi_msg_2_ptr->timeData->seconds;
    //     t.tm_year = spi_msg_2_ptr->timeData->year+100;
    //     t.tm_mon = spi_msg_2_ptr->timeData->month-1;
    //     t.tm_mday = spi_msg_2_ptr->timeData->date;
    // }
    
    

    dataOutput->timestamp  = (uint64_t)mktime(&t) * 1000LL;    
    dataOutput->timestamp = dataOutput->timestamp + live_data_buffer.timeData.subseconds;
    // ESP_LOGI(TAG_LOG, "%d %d, %d-%d-%d %d:%d:%d", msg_part, dataOutput->timestamp, t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

esp_err_t Logger_singleShot()
{
    // converted_reading_t measurement;
    uint8_t * spi_buffer = spi_ctrl_getRxData();

    spi_cmd_t cmd;

    cmd.command = STM32_CMD_SINGLE_SHOT_MEASUREMENT;
    cmd.data0 = 0;

    if (Logger_getState() == LOGTASK_IDLE)
    {
        if (spi_ctrl_cmd(STM32_CMD_SINGLE_SHOT_MEASUREMENT, &cmd, sizeof(spi_cmd_t)) == ESP_OK)
        {
            
            if (!((spi_buffer[0] == STM32_CMD_SINGLE_SHOT_MEASUREMENT) && (spi_buffer[1] == STM32_RESP_OK)))
            {
                spi_ctrl_print_rx_buffer(spi_buffer);
                ESP_LOGE(TAG_LOG, "Did not receive STM confirmation.");
                SET_ERROR(_errorCode,ERR_LOGGER_STM32_NO_RESPONSE);
                return ESP_FAIL;
            } 
            
        } else {
            ESP_LOGE(TAG_LOG, "Single shot command failed.");
            SET_ERROR(_errorCode, ERR_LOGGER_STM32_FAULTY_DATA);
            return ESP_FAIL;
        }
        return ESP_OK;        
    } 
    
    return ESP_FAIL;
    // else if (Logger_getState() == LOGTASK_LOGGING)
    // {
    //     // Data should be already available, since normal logging is enabled
    //     return ESP_OK;   
    // } else {
    //     return ESP_FAIL;
    // }
    
    
}

esp_err_t Logger_syncSettings()
{
    settings_persist_settings();
    // Send command to STM32 to go into settings mode
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Setting SETTINGS mode");
    #endif
    spi_buffer = spi_ctrl_getRxData();

    spi_cmd_t cmd;


    // if (spi_ctrl_datardy_int(0) != ESP_OK)
    // {
    //     SET_ERROR(_errorCode, ERR_LOGGER_SPI_CTRL_ERROR);
    //     return ESP_FAIL;
    // }

    cmd.command = STM32_CMD_SETTINGS_MODE;
    cmd.data0 = 0;
    

    if (spi_ctrl_cmd(STM32_CMD_SETTINGS_MODE, &cmd, sizeof(spi_cmd_t)) == ESP_OK)
    {
        // spi_ctrl_print_rx_buffer();
        if (spi_buffer[0] != STM32_CMD_SETTINGS_MODE || spi_buffer[1] != STM32_RESP_OK)
        {
            ESP_LOGE(TAG_LOG, "Unable to put STM32 into SETTINGS mode. ");
            spi_ctrl_print_rx_buffer(spi_buffer);
            SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
            return ESP_FAIL;
        } 
        #ifdef DEBUG_LOGGING
        ESP_LOGI(TAG_LOG, "SETTINGS mode enabled");
        #endif
    } else {
        return ESP_FAIL;
    }

    // Settings_t * settings = settings_get();

    cmd.command = STM32_CMD_SET_ADC_CHANNELS_ENABLED;
    cmd.data0 = settings_get_adc_channel_enabled_all();

    spi_ctrl_cmd(STM32_CMD_SET_ADC_CHANNELS_ENABLED, &cmd, sizeof(spi_cmd_t));
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_ADC_CHANNELS_ENABLED || spi_buffer[1] != STM32_RESP_OK)
    {
        ESP_LOGE(TAG_LOG, "Unable to set STM32 ADC channels. Received %d", spi_buffer[0]);
        spi_ctrl_print_rx_buffer(spi_buffer);
        SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
        return ESP_FAIL;
    }

    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "ADC channels set");
    #endif

    cmd.command = STM32_CMD_SET_RESOLUTION;
    cmd.data0 = (uint8_t)settings_get_resolution();

    spi_ctrl_cmd(STM32_CMD_SET_RESOLUTION, &cmd, sizeof(spi_cmd_t));
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_RESOLUTION || spi_buffer[1] != STM32_RESP_OK)
    {
        ESP_LOGE(TAG_LOG, "Unable to set STM32 ADC resolution. Received %d", spi_buffer[0]);
        spi_ctrl_print_rx_buffer(spi_buffer);
        SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
        return ESP_FAIL;
    } 
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "ADC resolution set");
    #endif
    
    cmd.command = STM32_CMD_SET_SAMPLE_RATE;
    cmd.data0 = (uint8_t)settings_get_samplerate();

    spi_ctrl_cmd(STM32_CMD_SET_SAMPLE_RATE, &cmd, sizeof(spi_cmd_t));
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_SAMPLE_RATE || spi_buffer[1] != STM32_RESP_OK )
    {
        ESP_LOGE(TAG_LOG, "Unable to set STM32 sample rate. ");
        spi_ctrl_print_rx_buffer(spi_buffer);
        SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
        return ESP_FAIL;
    }
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Sample rate set");
    #endif

    cmd.command = STM32_CMD_SET_DATETIME;
    // cmd.data0 = (uint8_t)settings_get_timestamp();
    uint32_t timestamp = settings_get_timestamp();
    memcpy(&cmd.data0, &timestamp, sizeof(timestamp));

    // Send settings one by one and confirm
    spi_ctrl_cmd(STM32_CMD_SET_DATETIME, &cmd, sizeof(spi_cmd_t));
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_DATETIME || spi_buffer[1] != STM32_RESP_OK )
    {
     
        ESP_LOGE(TAG_LOG, "Unable to set timestamp");
  
        spi_ctrl_print_rx_buffer(spi_buffer);
        SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
        return ESP_FAIL;
    }
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Timestamp set");
    #endif
    cmd.command = STM32_CMD_MEASURE_MODE;
    cmd.data0 = 0;

    // Send settings one by one and confirm
    spi_ctrl_cmd(STM32_CMD_MEASURE_MODE, &cmd, sizeof(spi_cmd_t));
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_MEASURE_MODE || spi_buffer[1] != STM32_RESP_OK )
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 in measure mode");
        spi_ctrl_print_rx_buffer(spi_buffer);
        SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
        return ESP_FAIL;
    }

    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Sync done");
    #endif
    // Exit settings mode 
    return ESP_OK;
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
            #ifdef DEBUG_LOGGING
            ESP_LOGI(TAG_LOG, "MODE press hold");
            #endif
            state = 1;
          }
        break;
            
        case 1:
            if(level)
            {
                #ifdef DEBUG_LOGGING
                ESP_LOGI(TAG_LOG, "MODE released");
                #endif
                state = 2;
                return 1;
            } 
        break;

        case 2:
            #ifdef DEBUG_LOGGING
            ESP_LOGI(TAG_LOG, "MODE reset");
            #endif
            state = 0;
            
        break;    
    }
    
    return 0;
}

esp_err_t LogTask_start()
{
    CLEAR_ERRORS(_errorCode);
    if (_currentLogTaskState == LOGTASK_IDLE || _currentLogTaskState == LOGTASK_ERROR_OCCURED)
    {
        // gpio_set_level(GPIO_ADC_EN, 1);
        // _nextLogTaskState = LOGTASK_LOGGING;
        _startLogTask = 1;
        return ESP_OK;
    } 
    else 
    {
        return ESP_FAIL;
    }
}

esp_err_t LogTask_stop()
{
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "LogTask_stop() called");
    #endif
     
    if (_currentLogTaskState == LOGTASK_LOGGING)
    {
        Logging_stop();
        return ESP_OK;
    } 
    else 
    {
        return ESP_FAIL;
    }
}

esp_err_t Logging_stop()
{
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Logging_stop() called");
    #endif
    _stopLogging = 1;
    return ESP_OK;
}

esp_err_t Logging_start()
{
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Logging_start() called");
    #endif
    _startLogging = 1;
    return ESP_OK;
}

size_t Logger_flush_buffer_to_sd_card_uint8(uint8_t * buffer, size_t size)
{
    #ifdef DEBUG_SDCARD
    ESP_LOGI(TAG_LOG, "Flusing buffer to SD card");
    #endif
    return fileman_write(buffer, size);
    
   
}

size_t Logger_flush_buffer_to_sd_card_csv(int32_t * adcData, size_t lenAdc, uint8_t * gpioData, size_t lenGpio, uint8_t * timeData, size_t lenTime, size_t datarows)
{
    #ifdef DEBUG_SDCARD
    ESP_LOGI(TAG_LOG, "Flusing CSV buffer to SD card");
    #endif
    return fileman_csv_write(adcData, lenAdc, gpioData, lenGpio, timeData ,lenTime, datarows);
    
}


// uint8_t Logger_raw_to_csv(uint8_t * buffer, size_t size, uint8_t log_counter)
uint8_t Logger_raw_to_csv(uint8_t log_counter, const uint8_t * adcData, size_t length, uint8_t range, uint8_t type)
{
     
        int j,x=0;
        uint32_t writeptr = 0;
        uint64_t channel_range, channel_offset;
        // uint16_t adc0, adc1=0;
        #ifdef DEBUG_SDCARD
        ESP_LOGI(TAG_LOG, "raw_to_csv log_counter %d", log_counter);
        #endif

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

        //ESP_LOGI(TAG_LOG, "ADC FP: %d %d %d %d", adc_buffer_fixed_point[0], adc_buffer_fixed_point[1] , adc_buffer_fixed_point[2], adc_buffer_fixed_point[3]);
    
        return RET_OK;
}

LoggerState_t Logger_getState()
{
    return _currentLogTaskState;
}

esp_err_t Logger_check_sdcard_free_space()
{
     // Flush buffer to sd card
    uint32_t free_space = esp_sd_card_get_free_space();
    if ( free_space < SDCARD_FREE_SPACE_MINIMUM_KB)
    {
        SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_NO_FREE_SPACE);
        ESP_LOGE(TAG_LOG, "Not sufficient disk space");
        return ESP_FAIL;
    } 
    else if (free_space < SDCARD_FREE_SPACE_WARNING_KB)
    {   
        #ifdef DEBUG_SDCARD 
        ESP_LOGW(TAG_LOG, "Warning, low disk space!");
        #endif
    }

    return ESP_OK;
}

esp_err_t Logger_flush_to_sdcard()
{

    if (Logger_check_sdcard_free_space() != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (settings_get_logmode() == LOGMODE_CSV)
    {
        // ESP_LOGI(TAG_LOG, "ADC fixed p %d, %d, %d", adc_buffer_fixed_point[0], adc_buffer_fixed_point[1], adc_buffer_fixed_point[2]);
        //             ESP_LOGI(TAG_LOG, "GPIO %d, %d, %d", sdcard_data.gpioData[0], sdcard_data.gpioData[1], sdcard_data.gpioData[2]);
        //             ESP_LOGI(TAG_LOG, "Sizes: %d, %d, %d", sizeof(adc_buffer_fixed_point), sizeof(sdcard_data.gpioData), sizeof(sdcard_data.timeData));
        #ifdef DEBUG_SDCARD
        ESP_LOGI(TAG_LOG, "Flusing buffer");
        #endif

        if (!Logger_flush_buffer_to_sd_card_csv(
            adc_buffer_fixed_point, (sizeof(adc_buffer_fixed_point)/sizeof(int32_t)),
            sdcard_data.gpioData, sizeof(sdcard_data.gpioData), 
            sdcard_data.timeData, (sizeof(sdcard_data.timeData)/sizeof(s_date_time_t)), 
            sdcard_data.datarows) ) // lenght of data
        {
            SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_WRITE_ERROR);
            return ESP_FAIL;
        }

    } else {
        size_t len = Logger_flush_buffer_to_sd_card_uint8((uint8_t*)&sdcard_data, SD_BUFFERSIZE) ;
        // if (len != SD_BUFFERSIZE)
        if (len != 1)
        {
            ESP_LOGE(TAG_LOG, "Raw write error. Returned: %d", len);
            SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_WRITE_ERROR);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
   
}

uint32_t Logger_getError()
{
    return _errorCode;
}

void LogTask_resetCounter()
{
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "LogTask resetCounter");
    #endif

    log_counter = 0;
    sdcard_data.datarows = 0;
    expected_msg_part = 0;
    msg_part = 0;
}

void LogTask_reset()
{
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "LogTask reset");
    #endif
    LogTask_resetCounter();
    _errorCode = 0;
}

esp_err_t Logger_processData()
{
    // there is data
    if (spi_msg_1_ptr->startByte[0] == 0xFA &&
        spi_msg_1_ptr->startByte[1] == 0xFB &&
        expected_msg_part == 0)
        {
            msg_part = 0;
            expected_msg_part = 1;
            // ESP_LOGI(TAG_LOG, "Start bytes found 1/2");
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

            sdcard_data.datarows += spi_msg_1_ptr->dataLen;
            #ifdef DEBUG_LOGGING
            ESP_LOGI(TAG_LOG, "Time: %d, %d, %d", sdcard_data.timeData[log_counter*sizeof(spi_msg_1_ptr->timeData)], sdcard_data.timeData[log_counter*sizeof(spi_msg_1_ptr->timeData)+1], sdcard_data.timeData[log_counter*sizeof(spi_msg_1_ptr->timeData)+2]);
            ESP_LOGI(TAG_LOG, "GPIO: %d, %d, %d", sdcard_data.gpioData[log_counter*sizeof(spi_msg_1_ptr->gpioData)], sdcard_data.gpioData[log_counter*sizeof(spi_msg_1_ptr->gpioData)+1], sdcard_data.gpioData[log_counter*sizeof(spi_msg_1_ptr->gpioData)+2]);
            ESP_LOGI(TAG_LOG, "ADC: %d, %d, %d, %d", sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)], sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)+1], sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)+2], sdcard_data.adcData[log_counter*sizeof(spi_msg_1_ptr->adcData)+3]);
            ESP_LOGI(TAG_LOG, "dataLen: %ld", sdcard_data.datarows);
            #endif
        } 
        else if (spi_msg_2_ptr->stopByte[0] == 0xFB &&
                spi_msg_2_ptr->stopByte[1] == 0xFA &&
                expected_msg_part == 1)
        {
            msg_part = 1;
            expected_msg_part = 0;
            // Now the order is reversed.
            // ESP_LOGI(TAG_LOG, "Start bytes found 2/2");
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
            sdcard_data.datarows += spi_msg_2_ptr->dataLen;
            #ifdef DEBUG_LOGGING
            ESP_LOGI(TAG_LOG, "Time: %d, %d, %d", sdcard_data.timeData[log_counter*sizeof(spi_msg_2_ptr->timeData)], sdcard_data.timeData[log_counter*sizeof(spi_msg_2_ptr->timeData)+1], sdcard_data.timeData[log_counter*sizeof(spi_msg_2_ptr->timeData)+2]);
            ESP_LOGI(TAG_LOG, "GPIO: %d, %d, %d", sdcard_data.gpioData[log_counter*sizeof(spi_msg_2_ptr->gpioData)], sdcard_data.gpioData[log_counter*sizeof(spi_msg_2_ptr->gpioData)+1], sdcard_data.gpioData[log_counter*sizeof(spi_msg_2_ptr->gpioData)+2]);
            ESP_LOGI(TAG_LOG, "ADC: %d, %d, %d, %d", sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)], sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)+1], sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)+2], sdcard_data.adcData[log_counter*sizeof(spi_msg_2_ptr->adcData)+3]);
            ESP_LOGI(TAG_LOG, "dataLen: %ld", sdcard_data.datarows);
            #endif
            // }
        }  else {
            //     // No start or stop byte found!
            ESP_LOGE(TAG_LOG, "No start or stop byte found! Expected message: %d, stop bytes: %d, %d and %d, %d", expected_msg_part, spi_msg_1_ptr->startByte[0], spi_msg_1_ptr->startByte[1], spi_msg_2_ptr->stopByte[0], spi_msg_2_ptr->stopByte[1]);
            SET_ERROR(_errorCode, ERR_LOGGER_STM32_FAULTY_DATA);
            return ESP_FAIL;
        }


        // Convert data to fixed point if necessary
        if (settings_get_logmode() == LOGMODE_CSV)
        {
            Logger_raw_to_csv(log_counter, sdcard_data.adcData+log_counter*sizeof(spi_msg_1_ptr->adcData), sizeof(spi_msg_1_ptr->adcData), 
            settings_get_adc_channel_range_all(), 
            settings_get_adc_channel_type_all());
        }
        
        log_counter++; // received bytes = log_counter*512
        
        // copy values to live buffer, else it might be overwritten during a SPI transaction!
        // could also replace this with a semaphore
        if (!msg_part)
        {
            memcpy(live_data_buffer.adcData, spi_msg_1_ptr->adcData, sizeof(live_data_buffer.adcData));
            live_data_buffer.gpioData = spi_msg_1_ptr->gpioData[0];
            live_data_buffer.timeData = spi_msg_1_ptr->timeData[0];
        } else {
            memcpy(live_data_buffer.adcData, spi_msg_2_ptr->adcData, sizeof(live_data_buffer.adcData));
            live_data_buffer.gpioData = spi_msg_2_ptr->gpioData[0];
            live_data_buffer.timeData = spi_msg_2_ptr->timeData[0];
        }
        
        return ESP_OK;
}

void Logger_disableADCen_and_Interrupt()
{
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Disabling ADC enable and data rdy interrupt");
    #endif
    gpio_set_level(GPIO_ADC_EN, 0);
    spi_ctrl_datardy_int(0);
}

void Logging_reset()
{
    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Logging reset");
    #endif
    _nextLoggingState = LOGGING_IDLE;
}

esp_err_t Logger_logging()
{   
    
    currtime_us = esp_timer_get_time();
    esp_err_t ret;

    if (_stopLogging)
    {
        Logger_disableADCen_and_Interrupt();
    }

    switch (_currentLoggingState)
    {

        case LOGGING_IDLE:
            // Nothing to do
            if (_startLogging)
            {
                _startLogging = 0;
                // Reset the spi controller
                spi_ctrl_reset_rx_state();
                stm32TimerTimeout = esp_timer_get_time();
                // enable data rdy interrupt pin
                spi_ctrl_datardy_int(1);
                // Enable logging at STM32
                #ifdef DEBUG_LOGGING
                ESP_LOGI(TAG_LOG, "Enabling ADC_EN");
                #endif
                if(!gpio_get_level(GPIO_ADC_EN))
                {
                    gpio_set_level(GPIO_ADC_EN, 1);
                }
                _nextLoggingState = LOGGING_WAIT_FOR_DATA_READY;
            }
        break;
            
       
        case LOGGING_WAIT_FOR_DATA_READY:
        {
            rxdata_state_t state;
                
            state = spi_ctrl_rxstate();

            // To add: timeout function. This, however, depends on the logging rate used. 
            // For now, the time out is set to 73 seconds, since at 1 Hz, the maximum fill time of the buffer is 70 seconds (it's size is 70)
            if (currtime_us - stm32TimerTimeout > (DATA_LINES_PER_SPI_TRANSACTION+3)*1000000)
            {
                ESP_LOGE(TAG_LOG, "STM32 timed out: %llu", currtime_us - stm32TimerTimeout);
                SET_ERROR(_errorCode, ERR_LOGGER_STM32_TIMEOUT);
                Logger_disableADCen_and_Interrupt();
                _nextLoggingState = LOGGING_ERROR;
                break;
            }

            if (state == RXDATA_STATE_DATA_READY)
            {
                
                spi_ctrl_queue_msg(NULL, sizeof(spi_msg_1_t));
                stm32TimerTimeout = esp_timer_get_time();
                _nextLoggingState = LOGGING_RX0_WAIT;
                // _nextLoggingState = LOGGING_START;
            } 
            else if (state == RXDATA_STATE_DATA_OVERRUN) 
            {
                #ifdef DEBUG_LOGGING
                ESP_LOGE(TAG_LOG, "Data overrun!");
                #endif
                SET_ERROR(_errorCode, ERR_LOGGER_DATA_OVERRUN);
                Logger_disableADCen_and_Interrupt();
                _nextLoggingState = LOGGING_ERROR;
            } 
            else if (_stopLogging)
            // state cannot be RXDATA_STATE_DATA_READY, because else we will queue another message for receiving the last ADC leading to a 
            // system crash
            {   
                #ifdef DEBUG_LOGGING
                ESP_LOGI(TAG_LOG, "Setting _stopLogging to 0.");
                #endif
                _stopLogging = 0;
                _nextLoggingState = LOGGING_DONE;
            } 
        }
       
        break;

        case LOGGING_RX0_WAIT:
        {
            ret = spi_ctrl_receive_data();
            if (ret == ESP_OK)
            {
                #ifdef DEBUG_LOGGING
                ESP_LOGI(TAG_LOG, "POP QUEUE, _dataReceived = 1");
                #endif
                _dataReceived = 1;
                
                // Check, for example, gpio data size to keep track if sdcard_data is full
                // Change in the future

                // Set RX state to NODATA
                spi_ctrl_reset_rx_state();
                
                // If in the meantime we got a stop logging request, then stop immediately, else we 
                // continue logging for too long
                if (_stopLogging)
                {
                    _stopLogging = 0;
                    _nextLoggingState = LOGGING_DONE;
                } else {
                    _nextLoggingState = LOGGING_WAIT_FOR_DATA_READY;
                }
                
                
            } else {
                // Timeout or some other error
                if (ret == ESP_ERR_TIMEOUT)
                {
                    SET_ERROR(_errorCode, ERR_LOGGER_STM32_TIMEOUT);
                }
                SET_ERROR(_errorCode, ERR_LOGGER_STM32_NO_RESPONSE);
                Logger_disableADCen_and_Interrupt();
               _nextLoggingState = LOGGING_ERROR;
            }
                                           
        }
        break;

        case LOGGING_DONE:
        case LOGGING_ERROR:
           

        break;

    }

    if (_nextLoggingState != _currentLoggingState)
    {
        #ifdef DEBUG_LOGGING
        ESP_LOGI(TAG_LOG, "LOGGING state changing from %d to %d", _currentLoggingState, _nextLoggingState);
        #endif
        _currentLoggingState = _nextLoggingState;
    }
                
    return ESP_OK;
    
    
}


void task_logging(void * pvParameters)
{
    

    // esp_err_t ret;
    CLEAR_ERRORS(_errorCode);
   
    uint32_t lastTick = 0;
    // Init STM32 ADC enable pin
    // gpio_set_direction(GPIO_DATA_RDY_PIN, GPIO_MODE_INPUT);

    spi_cmd_t spi_cmd;


    gpio_set_direction(GPIO_START_STOP_BUTTON, GPIO_MODE_INPUT);
    
    gpio_config_t adc_en_conf={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1<<GPIO_ADC_EN)
    };

    gpio_config(&adc_en_conf);
    gpio_set_level(GPIO_ADC_EN, 0);

    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_21, 1);

    gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_26, 1);


    // gpio_set_direction(STM32_SPI_CS, GPIO_MODE_OUTPUT);
    // gpio_set_level(STM32_SPI_CS, 0);

    
    // // Initialize SD card
    if (esp_sd_card_mount() == ESP_OK)
    {
        #ifdef DEBUG_LOGTASK
        ESP_LOGI(TAG_LOG, "File seq nr: %d", fileman_search_last_sequence_file());
        #endif
        esp_sd_card_unmount();
    } 
    
    #ifdef DEBUG_LOGTASK
    ESP_LOGI(TAG_LOG, "Logger task started");
    #endif

    if (spi_ctrl_init(STM32_SPI_HOST, GPIO_DATA_RDY_PIN) != ESP_OK)
    {
        // throw error
        ESP_LOGE(TAG_LOG, "Unable to initialize the SPI data controller!");
        while(1);
    }

    if (Logger_syncSettings() != ESP_OK)
    {
        ESP_LOGE(TAG_LOG, "STM32 settings FAILED");
    } else {
        #ifdef DEBUG_LOGTASK
        ESP_LOGI(TAG_LOG, "STM32 settings synced");
        #endif
    }

    
    spi_msg_1_ptr = (spi_msg_1_t*)spi_buffer;
    spi_msg_2_ptr = (spi_msg_2_t*)spi_buffer;
   spi_ctrl_datardy_int(0);

    while(1) {
       
    
        spi_ctrl_loop();
        Logger_logging();

        switch (_currentLogTaskState)
        {
            case LOGTASK_IDLE:
                // Give starting of logging priority over getting a single shot value.
                if (_startLogTask)
                {
                    _startLogTask = 0;
                    lastTick = 0;
                    if (esp_sd_card_mount() == ESP_OK)
                    {

                        if (Logger_check_sdcard_free_space() != ESP_OK)
                        {
                                esp_sd_card_unmount();
                            break;
                        }
                        #ifdef DEBUG_LOGTASK
                        ESP_LOGI(TAG_LOG, "File seq nr: %d", fileman_search_last_sequence_file());
                        #endif
                        if (fileman_open_file() != ESP_OK)
                        { 
                            if (esp_sd_card_unmount() == ESP_OK)
                            {
                                SET_ERROR(_errorCode, ERR_FILEMAN_UNABLE_TO_OPEN_FILE);
                                _nextLogTaskState = LOGTASK_IDLE;
                            }

                            break;
                        } 

                        
                        if (settings_get_logmode() == LOGMODE_CSV)
                        {
                            fileman_csv_write_header();
                        }
                            
                        // All good, put statemachines in correct state
                        _nextLogTaskState = LOGTASK_LOGGING;                        
                        // Reset and start the logging statemachine
                        
                        LogTask_reset();
                        Logging_reset();
                        Logging_start();
                        
                    } else {
                        SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_UNABLE_TO_MOUNT);
                        _nextLogTaskState = LOGTASK_IDLE;
                    }
                } else {
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    lastTick++;
                    if (lastTick > 1)
                    {
                        if (Logger_singleShot() == ESP_OK)
                        {
                            // Wait for the STM32 to acquire data. Takes about 40 ms.
                            vTaskDelay(50 / portTICK_PERIOD_MS);

                            // Wait a bit before requesting the data
                            spi_cmd.command = STM32_CMD_SEND_LAST_ADC_BYTES;

                            LogTask_resetCounter();
                            if (spi_ctrl_cmd(STM32_CMD_SEND_LAST_ADC_BYTES, &spi_cmd, sizeof(spi_msg_1_t)) == ESP_OK)
                            {
                                #ifdef DEBUG_LOGTASK_RX
                                ESP_LOGI(TAG_LOG, "Last msg received");
                                #endif
                                Logger_processData();
                                
                            } else {
                                ESP_LOGE(TAG_LOG, "Error receiving last message");
                            }
                        } else {
                            ESP_LOGE(TAG_LOG, "Singleshot error");
                        }
                        lastTick = 0;
                    }
                }
            
            break;

            case LOGTASK_LOGGING:     

                if (_dataReceived)           
                {
                    #ifdef DEBUG_LOGTASK_RX
                    ESP_LOGI(TAG_LOG, "Logtask: _dataReceived = 1");
                    #endif
                    if (Logger_processData() != ESP_OK)
                    {
                        LogTask_stop();
                        SET_ERROR(_errorCode, ERR_LOGGER_STM32_FAULTY_DATA);
                        _nextLogTaskState = LOGTASK_IDLE;
                    }
                    #ifdef DEBUG_LOGTASK_RX
                    ESP_LOGI(TAG_LOG, "_dataReceived = 0");
                    #endif
                    _dataReceived = 0;
                }
                
                // do we need to flush the data? 
                if(log_counter >= DATA_TRANSACTIONS_PER_SD_FLUSH)
                {
                    // No need to check for error, is done at _errorcode >0 check
                    Logger_flush_to_sdcard();
                    
                    log_counter = 0;
              
                    sdcard_data.datarows = 0;
                }

                if ((_currentLoggingState == LOGGING_ERROR ||
                    _errorCode > 0) &&
                    _currentLoggingState != LOGGING_DONE) 
                {
                    if (_currentLoggingState == LOGGING_ERROR || _errorCode > 0)
                    {
                        ESP_LOGE(TAG_LOG, "Error 0x%08lX occured in Logging statemachine. Stopping..", _errorCode);
                        LogTask_stop();
                    } 
                    
                }

                if ( _currentLoggingState == LOGGING_DONE)
                {
                    // Now either we already received the last message, which is indicated by _dataReceived
                    // or we will have to retrieve it. 
                    if ( _dataReceived )
                    {
                        #ifdef DEBUG_LOGTASK
                        ESP_LOGI(TAG_LOG, "LOGGING DONE and _dataReceived == 1. Processing data");
                        #endif
       
                        Logger_processData();
                        
                        #ifdef DEBUG_LOGTASK_RX
                        ESP_LOGI(TAG_LOG,"_dataReceived = 0");
                        #endif
                        _dataReceived = 0;
                    } else {
                        // Wait for STM to stop ADC and go to idle mode
                        vTaskDelay(200 / portTICK_PERIOD_MS);
                        spi_cmd.command = STM32_CMD_SEND_LAST_ADC_BYTES;
                        // Retrieve any remaining ADC bytes
                        
                        if (spi_ctrl_cmd(STM32_CMD_SEND_LAST_ADC_BYTES, &spi_cmd, sizeof(spi_msg_1_t)) == ESP_OK)
                        {
                            #ifdef DEBUG_LOGTASK_RX
                            ESP_LOGI(TAG_LOG, "Last ADC data received");
                            #endif
    
                            // Add check if last number bytes equals number of data lines. If so, we should discard that
                            Logger_processData();
                            
                        } else {
                            ESP_LOGE(TAG_LOG, "Error receiving last message");
                            SET_ERROR(_errorCode, ERR_LOGGER_STM32_FAULTY_DATA);
                        }
                        
                        Logger_flush_to_sdcard();
                        fileman_close_file();
                        esp_sd_card_unmount();
                        
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                        _nextLogTaskState = LOGTASK_IDLE;
                    }
                    
                   
                    break;
                }

            break;

            default:

            break;
            // should not come here
        }

         if (Logger_mode_button_pushed())
        {
            if (_currentLogTaskState == LOGTASK_IDLE || _currentLogTaskState == LOGTASK_ERROR_OCCURED)
            {
                LogTask_start();
            }
            
            if (_currentLogTaskState == LOGTASK_LOGGING)
            {
                LogTask_stop();
            }
        }


        if (_nextLogTaskState != _currentLogTaskState)
        {
            #ifdef DEBUG_LOGTASK
            ESP_LOGI(TAG_LOG, "Changing LOGTASK state from %d to %d", _currentLogTaskState, _nextLogTaskState);
            #endif
            _currentLogTaskState = _nextLogTaskState;
        }


    }
     
    
}

