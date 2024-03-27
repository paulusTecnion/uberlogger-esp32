#include "logger.h"
#include <stdio.h>
#include "common.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "esp_async_memcpy.h"
#include "errorcodes.h"
#include "fileman.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_sd_card.h"
#include "esp_wifi_types.h"
#include "config.h"
#include "settings.h"
#include "spi_control.h"
#include "sysinfo.h"
#include "tempsensor.h"
#include <time.h>
#include "wifi.h"

static const char* TAG_LOG = "LOGGER";

uint8_t *spi_buffer;
spi_msg_1_adc_only_t *spi_msg_1_adc_only_ptr;
spi_msg_2_adc_only_t *spi_msg_2_adc_only_ptr;
spi_msg_1_t *spi_msg_slow_freq_1_ptr;
spi_msg_2_t *spi_msg_slow_freq_2_ptr;
uint8_t _stopLogging = 0;
uint8_t _startLogging = 0;
uint8_t _startLogTask = 0;
uint8_t _stopLogTask = 0;

uint64_t first_tick = 0, first_tick2 = 0;

uint8_t userRequestsUnmount = 0;
uint8_t _dataReceived = 0;
uint64_t stm32TimerTimeout, currtime_us =0;

uint32_t free_space = 0;

async_memcpy_t driver = NULL;
SemaphoreHandle_t copy_done_sem;
SemaphoreHandle_t sdcard_semaphore;
SemaphoreHandle_t idle_state;

extern converted_reading_t live_data;

typedef struct
{
    uint8_t startByte[START_STOP_NUM_BYTES]; // 2
    uint16_t dataLen;
    s_date_time_t timeData; // 12*70 = 840
    uint8_t padding3;
    uint8_t padding4;
    uint8_t gpioData;    
    union {
        uint8_t adcData[16]; 
        uint16_t adcData16[8];
    };
   
} live_data_t;

live_data_t live_data_buffer;

uint8_t msg_part = 0, expected_msg_part = 0;

// struct {
//     uint8_t timeData[TIME_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
//     uint8_t adcData[ADC_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
//     uint8_t gpioData[GPIO_BYTES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH];
//     uint32_t datarows;
// }sdcard_data;

// Make a buffer of 4 times the size of spi_msg_1_t


sdcard_data_t sdcard_data;


// Same size as SPI buffer but then int32 size. 
int32_t adc_buffer_fixed_point[(ADC_VALUES_PER_SPI_TRANSACTION*DATA_TRANSACTIONS_PER_SD_FLUSH)];
int32_t adc_calibration_values[NUM_ADC_CHANNELS];

char strbuffer[16];


LoggerFWState_t _currentFWState = LOGGER_FW_IDLE;
LoggerFWState_t _nextFWState = LOGGER_FW_EMPTY_STATE;

// Really need to change all these variable names to something more sensible
LoggerState_t _currentLogTaskState = LOGTASK_IDLE;
LoggerState_t _nextLogTaskState = LOGTASK_IDLE;
LoggingState_t _currentLoggingState = LOGGING_IDLE;
LoggingState_t _nextLoggingState = LOGGING_IDLE;

// Queue for logging
QueueHandle_t xQueue = NULL;
// Queue for firmware update 
QueueHandle_t xQueueFW = NULL;

// temporary variables to calculate fixed point numbers
int64_t t0, t1,t2,t3;
uint32_t _errorCode;
static uint8_t _systemTimeSet = 0;

// Handle to stm32 task
extern TaskHandle_t xHandle_stm32;

// State of STM interrupt pin. 0 = low, 1 = high
// bool int_level = 0;
// Interrupt counter that tracks how many times the interrupt has been triggered. 


static uint8_t log_counter = 0;

// static portMUX_TYPE processDataSpinLock = portMUX_INITIALIZER_UNLOCKED;

static IRAM_ATTR bool async_memcpy_cb(async_memcpy_t mcp_hdl, async_memcpy_event_t *event, void *cb_args)
{
    // SemaphoreHandle_t sem = (SemaphoreHandle_t)cb_args;
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(copy_done_sem, &high_task_wakeup); // high_task_wakeup set to pdTRUE if some high priority task unblocked
    return (high_task_wakeup == pdTRUE);
}


int32_t Logger_convertAdcFixedPoint(int32_t adcVal, int64_t range, int64_t offset)
{
    // t0 = ((int32_t)adcData0 | ((int32_t)adcData1 << 8));
    t0 = (int64_t)adcVal;
            
    // In one buffer of STM_TXLENGTH bytes, there are only STM_TXLENGTH/2 16 bit ADC values. So divide by 2
    t1 = t0 * (-1LL*range); // note the minus for inverted input!
    if (settings_get_resolution() == ADC_12_BITS)
    {
        t2 = t1 / (4095); // 4096 - 1  steps
    } else {
        // For 16 bits, take this value (from datasheet STM32G030, page 305).
        t2 = t1 / (65520); //  65520 -1 steps
    }
    t3 = t2 + offset;
    return (int32_t) t3;
    
}


LoggerState_t LogTaskGetState()
{
    return _currentLogTaskState;
}

uint32_t LogTaskGetError()
{
    return _errorCode;
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
    // float tfloat = 0.0;

    int32_t adc0, adc1, fixed_pt_val; 

    int32_t channel_offset, channel_range;


    for (int i =0; i < 16; i=i+2)
    {
        adc0 = live_data_buffer.adcData[i];
        adc1 = live_data_buffer.adcData[i+1];
        int32_t adcVal = adc0 | (adc1 << 8);
        dataOutput->analogDataRaw[j] = adcVal;

        // compensate for offset
        //  ESP_LOGI(TAG_LOG, "adcVal:%u", adcVal);
        if (settings_get()->adc_resolution == ADC_12_BITS)
        {
            adcVal = adcVal + ((int32_t)(1<<11) - (int32_t)(settings_get()->adc_offsets_12b[j]));
        } else {
            adcVal = adcVal + ((int32_t)(32760) - (int32_t)(settings_get()->adc_offsets_16b[j])); // Max ADC is FFF0 (65520) if 16 bits
        }
        
        // The next values are calibrated values
        if (settings_get_adc_channel_range(settings_get(), j))
        {
            channel_offset = V_OFFSET_60V; //126811146; // 60*ADC_MULT_FACTOR_60V;

        } else {
            channel_offset = V_OFFSET_10V; //151703704; //10*ADC_MULT_FACTOR_10V;
            
        }

        channel_range = 2*channel_offset;

    
        // dataOutput->analogData[j] = Logger_convertAdcFixedPoint(adcVal, channel_range, channel_offset);


        if (settings_get_adc_channel_type(settings_get(),j) )
        {
            if (settings_get_resolution() == ADC_16_BITS)
            {
                adcVal = adcVal >> 4;
            } 
            dataOutput->temperatureData[j] = NTC_ADC2Temperature(adcVal);
        } else {
                // if range is 60V...
                dataOutput->analogData[j] = Logger_convertAdcFixedPoint(adcVal, channel_range, channel_offset);

            }

        // ESP_LOGI(TAG_LOG, "%u, %f", adcVal, dataOutput->analogData[j]);

        // ESP_LOGI(TAG_LOG, "%f", dataOutput->analogData[j]);
        

            
        // tfloat = (int32_t)NTC_ADC2Temperature(adcVal)/10.0F;
        
        
        // dataOutput->temperatureData[j] = tfloat;
        j++;
    }


        dataOutput->gpioData[0] = (live_data_buffer.gpioData & 0x04) && 1;
        dataOutput->gpioData[1] = (live_data_buffer.gpioData & 0x08) && 1;
        dataOutput->gpioData[2] = (live_data_buffer.gpioData & 0x10) && 1;
        dataOutput->gpioData[3] = (live_data_buffer.gpioData & 0x20) && 1;
        dataOutput->gpioData[4] = (live_data_buffer.gpioData & 0x40) && 1;
        dataOutput->gpioData[5] = (live_data_buffer.gpioData & 0x80) && 1;

        t.tm_hour = live_data_buffer.timeData.hours;
        t.tm_min =  live_data_buffer.timeData.minutes;
        t.tm_sec =  live_data_buffer.timeData.seconds;
        t.tm_year = live_data_buffer.timeData.year+100;
        t.tm_mon =  live_data_buffer.timeData.month-1;
        t.tm_mday = live_data_buffer.timeData.date;    

    dataOutput->timestamp  = (uint64_t)mktime(&t) * 1000LL;    
    dataOutput->timestamp = dataOutput->timestamp + (uint64_t)(live_data_buffer.timeData.subseconds);
    // ESP_LOGI(TAG_LOG, "%lld, %d-%d-%d %d:%d:%d",  dataOutput->timestamp, t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

esp_err_t Logger_singleShot()
{
    // converted_reading_t measurement;
    uint8_t * spi_buffer = spi_ctrl_getRxData();

    spi_cmd_t cmd;

    cmd.command = STM32_CMD_SINGLE_SHOT_MEASUREMENT;
    cmd.data0 = 0;

    // if (Logger_getState() == LOGTASK_SINGLE_SHOT)
    // {
        if (spi_ctrl_cmd(STM32_CMD_SINGLE_SHOT_MEASUREMENT, &cmd, sizeof(spi_cmd_t)) == ESP_OK)
        {
            if (settings_get_samplerate() <= ADC_SAMPLE_RATE_2Hz)
            {
                vTaskDelay(1500 / portTICK_PERIOD_MS);
            } 

            if (!((spi_buffer[0] == STM32_CMD_SINGLE_SHOT_MEASUREMENT) && (spi_buffer[1] == STM32_RESP_OK)))
            {
                spi_ctrl_print_rx_buffer(spi_buffer);
                ESP_LOGE(TAG_LOG, "Did not receive STM confirmation.");
                SET_ERROR(_errorCode,ERR_LOGGER_STM32_NO_RESPONSE);
                return ESP_FAIL;
            } 
            
        } else {
            // ESP_LOGE(TAG_LOG, "Single shot command failed.");
            
            // Reset the STM in case it's stuck (workaround)
            Logger_resetSTM32();
            vTaskDelay(300 / portTICK_PERIOD_MS);
            Logger_syncSettings(0);
            return ESP_FAIL;
        }
        return ESP_OK;        
    // } 
    
    // return ESP_FAIL;
    // else if (Logger_getState() == LOGTASK_LOGGING)
    // {
    //     // Data should be already available, since normal logging is enabled
    //     return ESP_OK;   
    // } else {
    //     return ESP_FAIL;
    // }
    
    
}

void  Logger_resetSTM32()
{
    gpio_set_level(GPIO_STM32_NRESET, 0);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_STM32_NRESET, 1);
    vTaskDelay(300 / portTICK_PERIOD_MS);
}

esp_err_t Logger_syncSettings(uint8_t syncTime)
{

    // Reset STM32

    // Logger_resetSTM32();

    vTaskDelay(300 / portTICK_PERIOD_MS);
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

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Settings_t * settings = settings_get();

    // cmd.command = STM32_CMD_SET_ADC_CHANNELS_ENABLED;
    // cmd.data0 = 0xFF; // Always for to use all channels //settings_get_adc_channel_enabled_all();

    // spi_ctrl_cmd(STM32_CMD_SET_ADC_CHANNELS_ENABLED, &cmd, sizeof(spi_cmd_t));
    // // spi_ctrl_print_rx_buffer();
    // if (spi_buffer[0] != STM32_CMD_SET_ADC_CHANNELS_ENABLED || spi_buffer[1] != STM32_RESP_OK)
    // {
    //     ESP_LOGE(TAG_LOG, "Unable to set STM32 ADC channels. Received %d", spi_buffer[0]);
    //     spi_ctrl_print_rx_buffer(spi_buffer);
    //     SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
    //     return ESP_FAIL;
    // }

    // #ifdef DEBUG_LOGGING
    // ESP_LOGI(TAG_LOG, "ADC channels set");
    // #endif

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

    cmd.command = STM32_CMD_SET_RANGE;
    cmd.data0 = (uint8_t)settings_get_adc_channel_range_all();

    spi_ctrl_cmd(STM32_CMD_SET_RANGE, &cmd, sizeof(spi_cmd_t));
    // spi_ctrl_print_rx_buffer();
    if (spi_buffer[0] != STM32_CMD_SET_RANGE || spi_buffer[1] != STM32_RESP_OK )
    {
        ESP_LOGE(TAG_LOG, "Unable to set STM32 voltage range. ");
        spi_ctrl_print_rx_buffer(spi_buffer);
        SET_ERROR(_errorCode, ERR_LOGGER_STM32_SYNC_ERROR);
        return ESP_FAIL;
    }

    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Voltage range set");
    #endif

    


    if (syncTime)
    {
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
    }


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

    // adc_channel_range_t ranges[NUM_ADC_CHANNELS];
    // for (int i=0; i<NUM_ADC_CHANNELS; i++)
    // {
    //     if(!settings_get_adc_channel_range(i))
    //     {
    //         ranges[i] = ADC_RANGE_10V;
    //     } else {
    //         ranges[i] = ADC_RANGE_60V;
    //     }
    // }


    // iir_set_settings(settings_get_samplerate(), ranges);
    // iir_reset();

    // Let STM load settings
    vTaskDelay(500 / portTICK_PERIOD_MS);

    #ifdef DEBUG_LOGGING
    ESP_LOGI(TAG_LOG, "Sync done");
    #endif
    // Exit settings mode 
    return ESP_OK;
}

esp_err_t Logtask_sync_settings()
{
    if (_currentLogTaskState != LOGTASK_IDLE && _currentLogTaskState != LOGTASK_SINGLE_SHOT)
    {
        ESP_LOGE(TAG_LOG, "Cannot sync settings while logging or calibration");
        return ESP_FAIL;
    }
    LoggingState_t t = LOGTASK_PERSIST_SETTINGS;
    if (xQueueSend(xQueue, &t, 0) != pdTRUE)
    {
        ESP_LOGE(TAG_LOG, "Unable to send sync settings command to queue");
        return ESP_FAIL;
    }

    t = LOGTASK_SYNC_SETTINGS;
    if (xQueueSend(xQueue, &t, 0) != pdTRUE)
    {
        ESP_LOGE(TAG_LOG, "Unable to send sync settings command to queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Logtask_sync_time()
{
    if (_currentLogTaskState == LOGTASK_LOGGING)
    {
        ESP_LOGE(TAG_LOG, "Cannot sync settings while logging");
        return ESP_FAIL;
    }
    LoggingState_t t = LOGTASK_PERSIST_SETTINGS;
    if (xQueueSend(xQueue, &t, 0) != pdTRUE)
    {
        ESP_LOGE(TAG_LOG, "Unable to send sync settings command to queue");
        return ESP_FAIL;
    }

    t = LOGTASK_SYNC_TIME;
    if (xQueueSend(xQueue, &t, 0) != pdTRUE)
    {
        ESP_LOGE(TAG_LOG, "Unable to send sync time command to queue");
        return ESP_FAIL;
    }

    return ESP_OK;  
}

esp_err_t Logtask_wifi_connect_ap()
{
    LoggingState_t t = LOGTASK_WIFI_CONNECT_AP;
    if (xQueueSend(xQueue, &t, 0) != pdTRUE)
    {
        ESP_LOGE(TAG_LOG, "Unable to connect to AP");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Logtask_wifi_disconnect_ap()
{
        LoggingState_t t = LOGTASK_WIFI_DISCONNECT_AP;
    if (xQueueSend(xQueue, &t, 0) != pdTRUE)
    {
        ESP_LOGE(TAG_LOG, "Unable to disconnect from AP");
        return ESP_FAIL;
    }
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

void Logger_mode_button_pushed()
{

    if (_currentLogTaskState == LOGTASK_IDLE || 
        _currentLogTaskState == LOGTASK_ERROR_OCCURED || 
        _currentLogTaskState == LOGTASK_SINGLE_SHOT)
    {
        LogTask_start();
        return;
    }
    
    if (_currentLogTaskState == LOGTASK_LOGGING)
    {
        LogTask_stop();
    }

}

void Logger_mode_button_long_pushed()
{
    if (_currentLogTaskState == LOGTASK_IDLE || 
        _currentLogTaskState == LOGTASK_ERROR_OCCURED ||
        _currentLogTaskState == LOGTASK_SINGLE_SHOT)
    {
        if (settings_get_wifi_mode()==WIFI_MODE_APSTA)
        {
            settings_set_wifi_mode(WIFI_MODE_AP);
            #ifdef DEBUG_LOGTASK
            ESP_LOGI(TAG_LOG, "Switching to AP mode");
            #endif
            
            
            // Push to queueu
            // settings_persist_settings();
            LoggerState_t t = LOGTASK_PERSIST_SETTINGS;
            xQueueSend(xQueue, &t, 1000/portTICK_PERIOD_MS);

            wifi_disconnect_ap();
        }
    }
   
}

esp_err_t Logger_format_sdcard()
{
    if (_currentLogTaskState == LOGTASK_IDLE ||
        _currentLogTaskState == LOGTASK_ERROR_OCCURED || 
        _currentLogTaskState == LOGTASK_SINGLE_SHOT)
    {
        LoggerState_t t = LOGTASK_FORMAT_SDCARD;
        xQueueSend(xQueue, &t, 1000/portTICK_PERIOD_MS);
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t LogTask_start()
{
    CLEAR_ERRORS(_errorCode);
    if ((_currentLogTaskState == LOGTASK_IDLE ||
        _currentLogTaskState == LOGTASK_ERROR_OCCURED || 
        _currentLogTaskState == LOGTASK_SINGLE_SHOT) && 
        xQueue != NULL)
    {
        // gpio_set_level(GPIO_ADC_EN, 1);
        // _nextLogTaskState = LOGTASK_LOGGING;
        LoggingState_t t = LOGTASK_LOGGING;
        xQueueSend(xQueue, &t, 1000/portTICK_PERIOD_MS);
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

// size_t Logger_flush_buffer_to_sd_card_csv(int32_t *adcData, size_t lenAdc, uint8_t *gpioData, size_t lenGpio, uint8_t *timeData, size_t lenTime, size_t datarows)
// {
// #ifdef DEBUG_SDCARD
//     ESP_LOGI(TAG_LOG, "Flusing CSV buffer to SD card");
// #endif
//     return fileman_csv_write(adcData, gpioData, timeData, datarows);
// }

esp_err_t Logger_calibrate()
{
    // #ifdef DEBUG_LOGGING
    if (_currentLogTaskState == LOGTASK_IDLE || 
        _currentLogTaskState == LOGTASK_SINGLE_SHOT)
        {
            ESP_LOGI(TAG_LOG, "Logger_calibrate() called");

            LoggingState_t t = LOGTASK_CALIBRATION;
            if (xQueueSend(xQueue, &t, 1000/portTICK_PERIOD_MS) != pdTRUE)
            {
                ESP_LOGE(TAG_LOG, "Unable to send calibrate command to queue");
                return ESP_FAIL;
            }
            return ESP_OK;
        }

        return ESP_FAIL;

}

// uint8_t Logger_raw_to_fixedpt(uint8_t * buffer, size_t size, uint8_t log_counter)
uint8_t Logger_raw_to_fixedpt(uint8_t log_counter, const uint16_t * adcData, size_t dataRows)
{
     
    int j,x=0;
    uint32_t writeptr = 0;
    int64_t channel_range=0, channel_offset=0;
    int32_t filtered_value=0;
    int32_t adcVal = 0;
    #ifdef DEBUG_SDCARD
    ESP_LOGI(TAG_LOG, "raw_to_csv log_counter %d", log_counter);
    #endif
    uint32_t length = 0;
    // Looping the ADC buffer is determined by dataRows as length = dataRows * 8 analog channels * 2 bytes
    length = dataRows * 8;
    
    if  ( settings_get()->adc_resolution == ADC_12_BITS )
    {
        
        // ESP_LOGI(TAG_LOG, "12 bits");
        for (j = 0; j < length; j++)
        {
            // Check for each channel what the range is (each bit in 'range' is 0 (=-10/+10V range) or 1 (=-60/+60V range) )
            // The next values are calibrated values
            if (settings_get_adc_channel_range(settings_get(), x))
            {
                // 60V range
                channel_offset = V_OFFSET_60V;

            } else {
                channel_offset = V_OFFSET_10V;   
            }

            channel_range = 2*channel_offset;
            
            // adcVal = (int32_t)((uint16_t)adcData[j] | ((uint16_t)adcData[j+1] << 8));
            adcVal = (int32_t)adcData[j];
            // Compenate offset
            adcVal = adcVal  + ( 2048 -settings_get()->adc_offsets_12b[x]);
            // Clip values
            if (adcVal < 0) adcVal = 0;
            if (adcVal > 4095) adcVal = 4095;

            // Detect type of sensor and conver accordingly
            if (settings_get_adc_channel_type(settings_get(), x))
            {
                int32_t temp = NTC_ADC2Temperature(adcVal);
                // ESP_LOGI(TAG_LOG,"temp %d, %ld", ((uint16_t)adcData[j] | ((uint16_t)adcData[j+1] << 8)), temp);
                adc_buffer_fixed_point[writeptr+(log_counter*ADC_VALUES_PER_SPI_TRANSACTION)] = temp;
            } else {
                adc_buffer_fixed_point[writeptr+(log_counter*ADC_VALUES_PER_SPI_TRANSACTION)] = Logger_convertAdcFixedPoint(adcVal, channel_range, channel_offset);   
            }
        

            x++;
            x = x % 8;
            writeptr++;
        }
    } 
    else
    { // 16 bit adc
    
        // IIR filter required for these cases
        for (j = 0; j < length; j++)
        {
                    // The next values are calibrated values
            if (settings_get_adc_channel_range(settings_get(), x))
            {
                // Bit == 1
                channel_offset = V_OFFSET_60V;  //126811146; //60*ADC_MULT_FACTOR_60V;
            } else {
                channel_offset = V_OFFSET_10V; //151703704; //10*ADC_MULT_FACTOR_10V;
            }
            channel_range = 2*channel_offset;
            // factor = ADC_16_BITS_60V_FACTOR;

            // adcVal = ((uint16_t)adcData[j] | ((uint16_t)adcData[j+1] << 8));
            adcVal = (int32_t)adcData[j];
            // Compenate offset
            adcVal = adcVal + ( (32760) - settings_get()->adc_offsets_16b[x]); // 32760 = 0xFFF0/2
            // Clip values
            if (adcVal < 0) adcVal = 0;
            // if (adcVal > (1<<16)-1) adcVal = (1<<16)-1;
            if (adcVal > (0xFFF0)) adcVal = (0xFFF0);

            // Detect type of sensor and conver accordingly
            if (settings_get_adc_channel_type(settings_get(), x))
            {
                // No lookup table for 16 bit! So we down covert it to 12 bit and use the LUT
                filtered_value = NTC_ADC2Temperature(adcVal >> 4);
            } else {
                filtered_value = Logger_convertAdcFixedPoint(adcVal, channel_range, channel_offset);    
            }
            
            adc_buffer_fixed_point[writeptr+(log_counter*ADC_VALUES_PER_SPI_TRANSACTION)] = filtered_value;
            
            x++;
            x = x % 8;
            writeptr++;
            
        }
        
    }
    
    
    

    return RET_OK;
}

LoggerState_t Logger_getState()
{
    return _currentLogTaskState;
}

uint32_t Logger_getLastFreeSpace()
{
    return free_space;
}

esp_err_t Logger_check_sdcard_free_space()
{
     // Flush buffer to sd card

    if (esp_sdcard_is_mounted())
    {
        free_space = esp_sd_card_get_free_space();
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
    } else {
        free_space = 0;
    }


    return ESP_OK;
}

esp_err_t Logger_flush_to_sdcard()
{
    //     if (fileman_open_file() == ESP_FAIL)
    // {
    //     SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_UNABLE_TO_OPEN_FILE);
    //     return ESP_FAIL;
    // }



    if (xSemaphoreTake(sdcard_semaphore, 600 / portTICK_PERIOD_MS) != pdTRUE) 
    {
        // did not get the sdcard write semaphore. Something really wrong!
        ESP_LOGE(TAG_LOG, "Error getting sdcard semaphore");
        goto error;
    }
    



    if (settings_get_logmode() == LOGMODE_CSV)
    {
        // ESP_LOGI(TAG_LOG, "ADC fixed p %d, %d, %d", adc_buffer_fixed_point[0], adc_buffer_fixed_point[1], adc_buffer_fixed_point[2]);
        //             ESP_LOGI(TAG_LOG, "GPIO %d, %d, %d", sdcard_data.gpioData[0], sdcard_data.gpioData[1], sdcard_data.gpioData[2]);
        //             ESP_LOGI(TAG_LOG, "Sizes: %d, %d, %d", sizeof(adc_buffer_fixed_point), sizeof(sdcard_data.gpioData), sizeof(sdcard_data.timeData));
        #ifdef DEBUG_SDCARD
        ESP_LOGI(TAG_LOG, "Flusing buffer");
        #endif
        ESP_LOGI(TAG_LOG, "Flusing buffer");
        if (fileman_csv_write_spi_msg(&sdcard_data, adc_buffer_fixed_point) != ESP_OK)
        {
            SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_WRITE_ERROR);
            goto error;
        }
    } else {
        // if (!Logger_flush_buffer_to_sd_card_csv(
        //     adc_buffer_fixed_point, (sizeof(adc_buffer_fixed_point)/sizeof(int32_t)),
        //     sdcard_data.gpioData, sizeof(sdcard_data.gpioData), 
        //     sdcard_data.timeData, (sizeof(sdcard_data.timeData)/sizeof(s_date_time_t)), 
        //     sdcard_data.datarows) ) // lenght of data
        // {
        //     SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_WRITE_ERROR);
        //     goto error;
        // }
        
        size_t len = Logger_flush_buffer_to_sd_card_uint8(sdcard_data.spi_data, sdcard_data.msgSize*sdcard_data.numSpiMessages);
        // if (len != SD_BUFFERSIZE)
        if (len != 1)
        {
            ESP_LOGE(TAG_LOG, "Raw write error. Returned: %d", len);
            SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_WRITE_ERROR);
            goto error;
        } else {
            sdcard_data.total_datarows += sdcard_data.datarows;
        }
    }

    // Check for file size and stop if we reached the limit
    
    if (fileman_check_current_file_size(settings_get_file_split_size()))
    {
        ESP_LOGI(TAG_LOG, "Reached max file size. Closing file and reopening new...");
        // write final row data bytes to partial files
        fileman_write(&(sdcard_data.total_datarows), sizeof(sdcard_data.total_datarows));
        fileman_close_file();
       
        fileman_set_prefix(settings_get_file_prefix(), live_data.timestamp, 1);
        // Set the total_datarows to 0.
        sdcard_data.total_datarows = 0;
        fileman_open_file();
        fileman_raw_write_header();
        //SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_MAX_FILE_SIZE_REACHED);
        //goto error;
    } 

    // Close file
    // if (fileman_close_file() == ESP_FAIL)
    // {
    //     SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_UNABLE_TO_CLOSE_FILE);
    //     return ESP_FAIL;
    // }

    // check if there's still space left (instead of doing this before every write)

    if (Logger_check_sdcard_free_space() != ESP_OK)
    {
        goto error;
    }


    xSemaphoreGive(sdcard_semaphore);

    return ESP_OK;

    error:
        xSemaphoreGive(sdcard_semaphore);
        return ESP_FAIL;
   
}

esp_err_t Logger_startFWupdate()
{
    if (((_currentLogTaskState == LOGTASK_IDLE) ||
        (_currentLogTaskState == LOGTASK_SINGLE_SHOT)))
    // if (xSemaphoreTake(idle_state, 1000 / portTICK_PERIOD_MS) != pdTRUE)
    {
       // #ifdef DEBUG_LOGGING
        ESP_LOGW(TAG_LOG, "Logger_startFWupdate: putting logger into LOGTASK_FWUPDATE..rebooting");
        // #endif
        LoggerState_t t = LOGTASK_FWUPDATE;
        if (xQueueSend(xQueue, &t, 1000 / portTICK_PERIOD_MS) != pdTRUE)
        {
            ESP_LOGE(TAG_LOG, "Logger_startFWupdate: Error sending to queue");
            return ESP_FAIL;
        }

        return ESP_OK;
    } else {
         // #ifdef DEBUG_LOGGING
        ESP_LOGW(TAG_LOG, "Logger_startFWupdate: Logger is not idle. Curent state: %d", _currentLogTaskState);
        // #endif
        return ESP_FAIL;
        
    }
}

esp_err_t Logger_startFWflash()
{
    LoggingState_t t;
    t = LOGGER_FW_START;
    xQueueSend(xQueueFW, &t, 1000 / portTICK_PERIOD_MS);
    return ESP_OK;
}

uint8_t Logger_getFWState ()
{
    return _currentFWState;
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
    sdcard_data.total_datarows = 0;
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

    // 2 scenarios:
    // - We have message using a slow frequency (<= 250 Hz) with the spi_msg_slow_freq_t struct
    // - Or we log with a high speed frequency (>500) with the structures spi_msg_adc_only_1_t and spi_msg_adc_only_2_t

    // Check what frequency we are logging with
    if (settings_get_samplerate() <= ADC_SAMPLE_RATE_250Hz)
    {
        // determine size of 1 message
       

        if (spi_msg_slow_freq_1_ptr->startByte[0] == 0xFA &&
            spi_msg_slow_freq_1_ptr->startByte[1] == 0xFB &&
            expected_msg_part == 0)
        {
            size_t msgSize = 4 + spi_msg_slow_freq_1_ptr->dataLen*sizeof(s_date_time_t) + spi_msg_slow_freq_1_ptr->dataLen + spi_msg_slow_freq_1_ptr->dataLen*2*8;
            msg_part = 0;
            expected_msg_part = 1;

          

            // Straight copy the data into the sdcard buffer
            if (spi_msg_slow_freq_1_ptr->dataLen >0 &&
                spi_msg_slow_freq_1_ptr->dataLen < 70 && 
                (settings_get_logmode() == LOGMODE_RAW))
            {
                // Copy startbytes and length
                memcpy(sdcard_data.spi_data + msgSize*log_counter, spi_msg_slow_freq_1_ptr->startByte, 2); 
                memcpy(sdcard_data.spi_data + msgSize*log_counter + 2, &(spi_msg_slow_freq_1_ptr->dataLen), 2);
                // Calculate the base offset for the current log entry
                size_t baseOffset = msgSize * log_counter + 4; // +4 to skip over the startBytes and dataLen which have already been copied

                // Copy time data block
                memcpy(sdcard_data.spi_data + baseOffset, spi_msg_slow_freq_1_ptr->timeData, sizeof(s_date_time_t) * spi_msg_slow_freq_1_ptr->dataLen);
                
                //ESP_LOGI("SPI MSG 1", "Time %d %d", spi_msg_slow_freq_1_ptr->timeData->minutes, spi_msg_slow_freq_1_ptr->timeData->seconds);
                // Copy gpio data block
                size_t gpioDataOffset = baseOffset + sizeof(s_date_time_t) * spi_msg_slow_freq_1_ptr->dataLen;
                memcpy(sdcard_data.spi_data + gpioDataOffset, spi_msg_slow_freq_1_ptr->gpioData, spi_msg_slow_freq_1_ptr->dataLen);

                // Copy adc data block
                size_t adcDataOffset = gpioDataOffset + spi_msg_slow_freq_1_ptr->dataLen;
                memcpy(sdcard_data.spi_data + adcDataOffset, spi_msg_slow_freq_1_ptr->adcData, spi_msg_slow_freq_1_ptr->dataLen * 2 * 8);
                
                sdcard_data.msgSize = msgSize;
                
            } else {
                ESP_ERROR_CHECK(esp_async_memcpy(driver, sdcard_data.spi_data + log_counter * sizeof(spi_msg_1_t), spi_msg_slow_freq_1_ptr, sizeof(spi_msg_1_t), async_memcpy_cb, NULL));
                sdcard_data.msgSize = sizeof(spi_msg_1_t);
                // Do something else here
                xSemaphoreTake(copy_done_sem, portMAX_DELAY); // Wait until the buffer copy is done
            }
            sdcard_data.datarows += spi_msg_slow_freq_1_ptr->dataLen;
                // Convert data if necessary
            if (settings_get_logmode() == LOGMODE_CSV)
            {
                // what needs to be done: take the ADC data and convert it
                spi_msg_1_t * spi_msg;
                spi_msg = (spi_msg_1_t *)(sdcard_data.spi_data + log_counter * sizeof(spi_msg_1_t));
                sdcard_data.msgSize = sizeof(spi_msg_1_t);
                Logger_raw_to_fixedpt(log_counter, spi_msg->adcData16, spi_msg->dataLen); 
            }
        } else if (spi_msg_slow_freq_2_ptr->stopByte[0] == 0xFB &&
                 spi_msg_slow_freq_2_ptr->stopByte[1] == 0xFA &&
                 expected_msg_part == 1) 
        {
            msg_part = 1;
            expected_msg_part = 0;

            if (
                spi_msg_slow_freq_1_ptr->dataLen >0 &&
                spi_msg_slow_freq_2_ptr->dataLen < 70 && 
                (settings_get_logmode() == LOGMODE_RAW))
            {
                size_t msgSize = 4 + spi_msg_slow_freq_2_ptr->dataLen*sizeof(s_date_time_t) + spi_msg_slow_freq_2_ptr->dataLen + spi_msg_slow_freq_2_ptr->dataLen*2*8;
                size_t baseOffset = msgSize * log_counter; 

                 // Copy adc data block
                memcpy(sdcard_data.spi_data + baseOffset, spi_msg_slow_freq_2_ptr->adcData, spi_msg_slow_freq_2_ptr->dataLen * 2 * 8);

                  // Copy gpio data block
                size_t gpioDataOffset = baseOffset + spi_msg_slow_freq_2_ptr->dataLen * 2 * 8;
                memcpy(sdcard_data.spi_data + gpioDataOffset, spi_msg_slow_freq_2_ptr->gpioData, spi_msg_slow_freq_2_ptr->dataLen);
                
                size_t timeDataOffset = gpioDataOffset + spi_msg_slow_freq_2_ptr->dataLen;
                // Copy time data block
                memcpy(sdcard_data.spi_data + timeDataOffset, spi_msg_slow_freq_2_ptr->timeData, sizeof(s_date_time_t) * spi_msg_slow_freq_2_ptr->dataLen);
                //ESP_LOGI("SPI MSG 2", "Time %d %d", spi_msg_slow_freq_2_ptr->timeData->minutes, spi_msg_slow_freq_2_ptr->timeData->seconds);
                size_t dataLenOffset = timeDataOffset + sizeof(s_date_time_t) * spi_msg_slow_freq_2_ptr->dataLen;
                // Copy stopbytes and length
                memcpy(sdcard_data.spi_data + dataLenOffset, &(spi_msg_slow_freq_2_ptr->dataLen), 2);
                memcpy(sdcard_data.spi_data + dataLenOffset + 2, spi_msg_slow_freq_2_ptr->stopByte, 2); 
                
            } else {
                // Straight copy the data into the sdcard buffer
                ESP_ERROR_CHECK(esp_async_memcpy(driver, sdcard_data.spi_data + log_counter * sizeof(spi_msg_2_t), spi_msg_slow_freq_2_ptr, sizeof(spi_msg_2_t), async_memcpy_cb, NULL));
                sdcard_data.msgSize = sizeof(spi_msg_2_t);
                // Do something else here
                xSemaphoreTake(copy_done_sem, portMAX_DELAY); // Wait until the buffer copy is done

            }

             sdcard_data.datarows += spi_msg_slow_freq_2_ptr->dataLen;
            if (settings_get_logmode() == LOGMODE_CSV)
            {
                    spi_msg_2_t * spi_msg;
                    spi_msg = (spi_msg_2_t *)(sdcard_data.spi_data + log_counter * sizeof(spi_msg_2_t));
                    Logger_raw_to_fixedpt(log_counter, spi_msg->adcData16, spi_msg->dataLen);
            }
        }
    }
    else
    {
        // there is data
        if (spi_msg_1_adc_only_ptr->startByte[0] == 0xFA &&
            spi_msg_1_adc_only_ptr->startByte[1] == 0xFB &&
            expected_msg_part == 0)
        {
            msg_part = 0;
            expected_msg_part = 1;
            // ESP_LOGI(TAG_LOG, "Start bytes found 1/2");
            // In this case we have Time bytes first...

            // copy all data from spi buffer to sdcard buffer
            ESP_ERROR_CHECK(esp_async_memcpy(driver, sdcard_data.spi_data + log_counter * sizeof(spi_msg_1_adc_only_t), spi_msg_1_adc_only_ptr, sizeof(spi_msg_1_adc_only_t), async_memcpy_cb, NULL));

            // Do something else here
            xSemaphoreTake(copy_done_sem, portMAX_DELAY); // Wait until the buffer copy is done

            // ESP_LOGI(TAG_LOG, "Done");
            sdcard_data.datarows += spi_msg_1_adc_only_ptr->dataLen;

            
            ESP_LOGI(TAG_LOG, "Data rows: %lu", sdcard_data.datarows);
#ifdef DEBUG_LOGGING
            spi_msg_slow_freq_t *spi_msg = (spi_msg_1_adc_only_t *)(sdcard_data.spi_data + log_counter * sizeof(spi_msg_1_adc_only_t));
            ESP_LOGI(TAG_LOG, "Time: %d, %d, %d", sdcard_data.timeData[log_counter * sizeof(spi_msg_1_ptr->timeData)], sdcard_data.timeData[log_counter * sizeof(spi_msg_1_ptr->timeData) + 1], sdcard_data.timeData[log_counter * sizeof(spi_msg_1_ptr->timeData) + 2]);
            ESP_LOGI(TAG_LOG, "GPIO: %d, %d, %d", sdcard_data.gpioData[log_counter * sizeof(spi_msg_1_ptr->gpioData)], sdcard_data.gpioData[log_counter * sizeof(spi_msg_1_ptr->gpioData) + 1], sdcard_data.gpioData[log_counter * sizeof(spi_msg_1_ptr->gpioData) + 2]);
            ESP_LOGI(TAG_LOG, "ADC: %d, %d, %d, %d", sdcard_data.adcData[log_counter * sizeof(spi_msg_1_ptr->adcData)], sdcard_data.adcData[log_counter * sizeof(spi_msg_1_ptr->adcData) + 1], sdcard_data.adcData[log_counter * sizeof(spi_msg_1_ptr->adcData) + 2], sdcard_data.adcData[log_counter * sizeof(spi_msg_1_ptr->adcData) + 3]);
            ESP_LOGI(TAG_LOG, "dataLen: %ld", sdcard_data.datarows);
#endif
        }
        else if (spi_msg_2_adc_only_ptr->stopByte[0] == 0xFB &&
                 spi_msg_2_adc_only_ptr->stopByte[1] == 0xFA &&
                 expected_msg_part == 1)
        {
            msg_part = 1;
            expected_msg_part = 0;
            // Now the order is reversed.
            // ESP_LOGI(TAG_LOG, "Start bytes found 2/2");
            // copy all data from spi buffer to sdcard buffer
            ESP_ERROR_CHECK(esp_async_memcpy(driver, sdcard_data.spi_data + log_counter * sizeof(spi_msg_2_adc_only_t), spi_msg_2_adc_only_ptr, sizeof(spi_msg_2_adc_only_t), async_memcpy_cb, NULL));

            // Do something else here
            xSemaphoreTake(copy_done_sem, portMAX_DELAY); // Wait until the buffer copy is done

            // ESP_LOGI(TAG_LOG, "Done");
            sdcard_data.datarows += spi_msg_2_adc_only_ptr->dataLen;
            ESP_LOGI(TAG_LOG, "Data rows: %lu", sdcard_data.datarows);
#ifdef DEBUG_LOGGING
            ESP_LOGI(TAG_LOG, "Time: %d, %d, %d", sdcard_data.timeData[log_counter * sizeof(spi_msg_2_ptr->timeData)], sdcard_data.timeData[log_counter * sizeof(spi_msg_2_ptr->timeData) + 1], sdcard_data.timeData[log_counter * sizeof(spi_msg_2_ptr->timeData) + 2]);
            ESP_LOGI(TAG_LOG, "GPIO: %d, %d, %d", sdcard_data.gpioData[log_counter * sizeof(spi_msg_2_ptr->gpioData)], sdcard_data.gpioData[log_counter * sizeof(spi_msg_2_ptr->gpioData) + 1], sdcard_data.gpioData[log_counter * sizeof(spi_msg_2_ptr->gpioData) + 2]);
            ESP_LOGI(TAG_LOG, "ADC: %d, %d, %d, %d", sdcard_data.adcData[log_counter * sizeof(spi_msg_2_ptr->adcData)], sdcard_data.adcData[log_counter * sizeof(spi_msg_2_ptr->adcData) + 1], sdcard_data.adcData[log_counter * sizeof(spi_msg_2_ptr->adcData) + 2], sdcard_data.adcData[log_counter * sizeof(spi_msg_2_ptr->adcData) + 3]);
            ESP_LOGI(TAG_LOG, "dataLen: %ld", sdcard_data.datarows);
#endif
            // }
        }
        else
        {
            //     // No start or stop byte found!
            ESP_LOGE(TAG_LOG, "No start or stop byte found! Expected message: %d, stop bytes: %d, %d and %d, %d", expected_msg_part, spi_msg_1_adc_only_ptr->startByte[0], spi_msg_1_adc_only_ptr->startByte[1], spi_msg_2_adc_only_ptr->stopByte[0], spi_msg_2_adc_only_ptr->stopByte[1]);
            SET_ERROR(_errorCode, ERR_LOGGER_STM32_FAULTY_DATA);
            return ESP_FAIL;
        }
    }


    log_counter++; // received bytes = log_counter*512
    sdcard_data.numSpiMessages = log_counter;

    // copy values to live buffer, else it might be overwritten during a SPI transaction!
    // could also replace this with a semaphore
    if (!msg_part)
    {
        memcpy(live_data_buffer.adcData16, spi_msg_slow_freq_1_ptr->adcData16, sizeof(live_data_buffer.adcData16));
        live_data_buffer.gpioData = spi_msg_slow_freq_1_ptr->gpioData[0];
        live_data_buffer.timeData = spi_msg_slow_freq_1_ptr->timeData[0];
    }
    else
    {
        memcpy(live_data_buffer.adcData16, spi_msg_slow_freq_2_ptr->adcData16, sizeof(live_data_buffer.adcData16));
        live_data_buffer.gpioData = spi_msg_slow_freq_2_ptr->gpioData[0];
        live_data_buffer.timeData = spi_msg_slow_freq_2_ptr->timeData[0];
    }

    return ESP_OK;
}

void Logger_disableADCen_and_Interrupt()
{


    // if (gpio_get_level(GPIO_ADC_EN) == 0)
    // {
    //     // Already disabled
    //     return;
    // }
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

esp_err_t Logging_restartSystem()
{
     LoggingState_t t = LOGTASK_REBOOT_SYSTEM;
    if (xQueueSend(xQueue, &t, 0) != pdTRUE)
    {
        ESP_LOGE(TAG_LOG, "Unable to send sync settings command to queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Logger_user_unmount_sdcard()
{
    // Check if system is logging now
    if (_currentLogTaskState == LOGTASK_LOGGING || 
        esp_sd_card_check_for_card() != ESP_OK)
    {
        return ESP_FAIL;
    } else {
        userRequestsUnmount = 1;
        return ESP_OK;
    }
}

esp_err_t Logging_check_sdcard()
{
    // Check if sd card is inserted
    if (esp_sd_card_check_for_card() == ESP_OK)
    {
        // Sd card inserted, mounted and user wants to unmount
        if (esp_sdcard_is_mounted() && userRequestsUnmount)
        {
            ESP_LOGI(TAG_LOG, "Unmounting SD card");
            esp_sd_card_unmount();
            Logger_check_sdcard_free_space();
        } 
        // sd card inserted and not mounted and last user action was not to unmount it => Mount it
        else if (!esp_sdcard_is_mounted() && !userRequestsUnmount)
        {
            ESP_LOGI(TAG_LOG, "Mounting SD card");
            if (esp_sd_card_mount() != ESP_OK)
            {
                SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_UNABLE_TO_MOUNT);
                return ESP_FAIL;
            } else {
                // check for free disk space
                Logger_check_sdcard_free_space();
            }
        }
    } 
    else 
    {
        // No sd card inserted. 
        // clear userRequestsUnmount
        userRequestsUnmount = 0;
        // if sd card was mounted, then unmount it. 
        if (esp_sdcard_is_mounted())
        {
            ESP_LOGI(TAG_LOG, "Unmounting SD card");
            esp_sd_card_unmount();
            Logger_check_sdcard_free_space();
        }
    }
   
    return ESP_OK;
   
}

esp_err_t Logger_logging()
{   
    
    currtime_us = esp_timer_get_time();
    esp_err_t ret;


    if (_nextLoggingState != _currentLoggingState)
    {
        #ifdef DEBUG_LOGGING
        ESP_LOGI(TAG_LOG, "LOGGING state changing from %d to %d", _currentLoggingState, _nextLoggingState);
        #endif
        _currentLoggingState = _nextLoggingState;
    }

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
            // For now, the time out is set to 73 seconds, since at 1 Hz, the maximum fill time of the buffer is 70 seconds (its size is 70)
            if (currtime_us - stm32TimerTimeout > (DATA_LINES_PER_SPI_TRANSACTION+3)*1000000)
            {
                ESP_LOGE(TAG_LOG, "STM32 timed out: %llu", currtime_us - stm32TimerTimeout);
                SET_ERROR(_errorCode, ERR_LOGGER_STM32_TIMEOUT);
                Logger_disableADCen_and_Interrupt();
                _nextLoggingState = LOGGING_DONE;
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
                _nextLoggingState = LOGGING_DONE;
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
               _nextLoggingState = LOGGING_DONE;
            }
                                           
        }
        break;

        case LOGGING_DONE:
        case LOGGING_ERROR:
          

        break;

    }

   
                
    return ESP_OK;
    
    
}


void Logtask_calibration()
{
    uint8_t calibration = 0, calibrationCounter = 0;
    int32_t calibrationValues[NUM_ADC_CHANNELS];
    adc_resolution_t last_resolution;
    adc_sample_rate_t last_sample_rate;
    uint8_t x = 0;

    last_resolution = settings_get_resolution();
    last_sample_rate = settings_get_samplerate();
    ESP_LOGI(TAG_LOG, "Calibration step %d", calibration);
    // Switch to 12 bits mode and do single shot. Then switch to 16 bits and the same.
    last_resolution = settings_get_resolution();
    last_sample_rate = settings_get_samplerate();

    while (1)
    {
        switch (calibration)
        {
            case 0:
                // Switch to 12 bits mode and do single shot. Then switch to 16 bits and the same.
                settings_set_resolution(ADC_12_BITS);
                settings_set_samplerate(ADC_SAMPLE_RATE_250Hz);
                settings_persist_settings();
                Logger_syncSettings(0);
                // Wait to let the stm32 make the configuration run
                vTaskDelay(500 / portTICK_PERIOD_MS);

                calibration = 1;
                _nextLogTaskState = LOGTASK_SINGLE_SHOT;
                for (uint8_t i = 0; i < NUM_ADC_CHANNELS; i++)
                {
                    calibrationValues[i] = 0;
                }

            break;

            case 1: 
            case 2:
                // Retrieve the calibration values
                
                // increase counter
                // calibrationCounter++;
              
                

                for (calibrationCounter = 1; calibrationCounter <= NUM_CALIBRATION_VALUES; calibrationCounter++)
                {  
                    ESP_LOGI(TAG_LOG, "Calibration counter: %d", calibrationCounter);
                    x = 0;
                    Logtask_singleShot();
                    
                    for (uint8_t i = 0; i < NUM_ADC_CHANNELS*2; i = i + 2)
                    {
                        // (uint16_t)adcData[j] | ((uint16_t)adcData[j+1] << 8)
                        calibrationValues[x] += ((uint16_t)spi_msg_slow_freq_1_ptr->adcData[i]) | ((uint16_t)spi_msg_slow_freq_1_ptr->adcData[i+1] << 8); 
                        ESP_LOGI(TAG_LOG, "Calibration value %u: %lu", x, calibrationValues[x]);
                        x++;
                    }

                     // Sometimes we get only zeros here due to ADC being a bit slow with starting up (especially for 16-bits). Quick fix for now.
                    if (calibrationValues[--x] == 0)
                    {
                        calibrationCounter--;
                    }

                
                    // store calibration values
                    // for (uint8_t i = 0; i < NUM_ADC_CHANNELS; i++)
                    // {
                    //     calibrationValues[i] = calibrationValues[i] / calibrationCounter;
                    //     ESP_LOGI(TAG_LOG, "Average calib value %u: %lu", i, calibrationValues[i]);
                    // }
                    
                    if (calibrationCounter == NUM_CALIBRATION_VALUES)
                    {
                        // store calibration values
                        for (uint8_t i = 0; i < NUM_ADC_CHANNELS; i++)
                        {
                            calibrationValues[i] = calibrationValues[i] / calibrationCounter;
                            ESP_LOGI(TAG_LOG, "Average calib value %u: %lu", i, calibrationValues[i]);
                        }
                        
                        if (calibration == 1)
                        {
                            settings_set_adc_offset(calibrationValues, ADC_12_BITS);
                        } else if (calibration == 2) {
                            settings_set_adc_offset(calibrationValues, ADC_16_BITS);
                        }
                        
                        settings_persist_settings();

                        // go to next state
                        
                        

                        for (uint8_t i = 0; i < NUM_ADC_CHANNELS; i++)
                        {
                            calibrationValues[i] = 0;
                        }

                       if (calibration == 1)
                        {
                            settings_set_resolution(ADC_16_BITS);
                            // settings_persist_settings();
                            Logger_syncSettings(0);
                            // Wait for stm32 to make configuration run and 
                            // let the IIR filter settle
                            vTaskDelay(2000 / portTICK_PERIOD_MS);
                            Logtask_singleShot();
                            // Make calibrationCounter 0, so that at next iteration it becomes 1 again.
                            calibrationCounter = 0;
                            calibration = 2;
                            ESP_LOGI(TAG_LOG, "Calibration step %d", calibration);
                        } else {
                            settings_set_resolution(last_resolution);
                            settings_set_samplerate(last_sample_rate);
                            settings_persist_settings();
                            Logger_syncSettings(0);
                            ESP_LOGI(TAG_LOG, "Calibration done");
                            calibration = 0;
                            return;
                        }
                        
                    } 
                }                

            break;

        }
    }
}

void Logtask_singleShot()
{
    spi_cmd_t spi_cmd;
    if (Logger_singleShot() == ESP_OK)
    {
        // Wait for the STM32 to acquire data. Takes about 40 ms.
        if (settings_get_samplerate() == ADC_SAMPLE_RATE_1Hz)
        {
            vTaskDelay(400 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        
        // Wait a bit before requesting the data
        spi_cmd.command = STM32_CMD_SEND_LAST_ADC_BYTES;

        LogTask_resetCounter();
        // ESP_LOGI(TAG_LOG, "Getting data of size %d", sizeof(spi_msg_slow_freq_t));
        if (spi_ctrl_cmd(STM32_CMD_SEND_LAST_ADC_BYTES, &spi_cmd, sizeof(spi_msg_1_t)) == ESP_OK)
        {
#ifdef DEBUG_LOGTASK_RX
// ESP_LOGI(TAG_LOG, "Last msg received");
#endif
            Logger_processData();
            Logger_GetSingleConversion(&live_data);
            // Set the system time if this is the first time we do a single shot
            if (!_systemTimeSet)
            {
                settings_set_system_time(live_data.timestamp/1000);
                _systemTimeSet = 1;
            }
                
        } else {
            ESP_LOGE(TAG_LOG, "Error receiving last message");
        }
    }
    else
    {
        // ESP_LOGE(TAG_LOG, "Singleshot error");
    }

    return;
                        
}

void Logtask_logging()
{
    esp_err_t ret;
    spi_cmd_t spi_cmd;
    uint8_t finalwrite = 0;

    if (esp_sdcard_is_mounted() )
    {

        if (Logger_check_sdcard_free_space() != ESP_OK)
        {
            // esp_sd_card_unmount();
            SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_NO_FREE_SPACE);
            // push error to queue?
            // ...
            return;
        }

        fileman_set_prefix(settings_get_file_prefix(), live_data.timestamp, 0);
        
        if (fileman_open_file() != ESP_OK)
        { 
            // esp_sd_card_unmount();
            SET_ERROR(_errorCode, ERR_FILEMAN_UNABLE_TO_OPEN_FILE);
            return;
            // push error to queue?
            // ...
        } 

        
        if (settings_get_logmode() == LOGMODE_CSV)
        {
            fileman_csv_write_header();
        } else {
            // write the ADC settings to the file when writing a raw file
            fileman_raw_write_header();
        }
                      
        // Reset and start the logging statemachine
        
        LogTask_reset();
        Logging_reset();
        Logging_start();
        
    } else {
        SET_ERROR(_errorCode, ERR_LOGGER_SDCARD_UNABLE_TO_MOUNT);
        // push error to queue?
        // ...
        return;
    }
    
    while (1)
    {
        // Put this in separate task?
        spi_ctrl_loop();
        Logger_logging();
        
        if (_dataReceived)           
        {
            #ifdef DEBUG_LOGTASK_RX
            ESP_LOGI(TAG_LOG, "Logtask: _dataReceived = 1");
            #endif
            
            // ESP_LOGI(TAG_LOG, "Time to process data: %lld", esp_timer_get_time() - first_tick2);
            // first_tick2 = esp_timer_get_time();
            ret = Logger_processData();
        
            
            if (ret != ESP_OK)
            {
                LogTask_stop();
                SET_ERROR(_errorCode, ERR_LOGGER_STM32_FAULTY_DATA);
                finalwrite = 1;
            }
            Logger_GetSingleConversion(&live_data);
        
            #ifdef DEBUG_LOGTASK_RX
            // ESP_LOGI(TAG_LOG, "_dataReceived = 0");
            #endif
            _dataReceived = 0;
        }
        
        // do we need to flush the data? 
        if(log_counter >= DATA_TRANSACTIONS_PER_SD_FLUSH)
        {
            if (Logger_flush_to_sdcard() != ESP_OK)
            {
                ESP_LOGE(TAG_LOG, "Error 0x%08lX occured in Logging statemachine. Stopping..", _errorCode);
                LogTask_stop();
                finalwrite = 1;
            } 
 
            log_counter = 0;
    
            sdcard_data.datarows = 0;
        }

        // Keep in mind we are talking about _currentLoggingState here, not _CurrentLogTaskState!
        if ((_currentLoggingState == LOGGING_ERROR ||
            _errorCode > 0) &&
            _currentLoggingState != LOGGING_DONE) 
        {
            if (_currentLoggingState == LOGGING_ERROR || _errorCode > 0)
            {
                ESP_LOGE(TAG_LOG, "Error 0x%08lX occured in Logging statemachine. Stopping..", _errorCode);
                LogTask_stop();
                // finalwrite = 1;
            } 
            
        }

        if ( _currentLoggingState == LOGGING_DONE)
        {
            // Now either we already received the last message, which is indicated by _dataReceived
            // or we will have to retrieve it.
            if (_dataReceived)
            {
#ifdef DEBUG_LOGTASK
                ESP_LOGI(TAG_LOG, "LOGGING DONE and _dataReceived == 1. Processing data");
#endif
                Logger_processData();
#ifdef DEBUG_LOGTASK_RX

#endif
                _dataReceived = 0;
            }
            // Flush the data to SD card
            finalwrite = 1;
            
        }

        if (finalwrite)
        {
            Logger_flush_to_sdcard();
            if (settings_get_logmode() == LOGMODE_RAW)
            {
                // Write total number of rows at the end of the file
                fileman_write(&(sdcard_data.total_datarows), sizeof(sdcard_data.total_datarows));
            }
            fileman_close_file();
            // esp_sd_card_unmount();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            // Exit the logging function
            ESP_LOGI(TAG_LOG, "Logtask_logging() exiting..");
            return;
        }

    }
}

void Logtask_fw_update_exit()
{
    _nextFWState = LOGGER_FW_ERROR;  
    xQueueSend(xQueueFW, &_nextFWState, 0);
}


void Logtask_fw_update()
{
    

    while(1)
    {
        if (xQueueReceive(xQueueFW, &_currentFWState, 200 / portTICK_PERIOD_MS) != pdTRUE)
        {
            _currentFWState = LOGGER_FW_IDLE;
        } 

        switch (_currentFWState)
        {    
            case LOGGER_FW_IDLE:            break;

            case LOGGER_FW_START:
                settings_set_boot_reason(1); 
                esp_restart();
            break;

            case LOGGER_FW_FLASHING_STM:
            if (esp_sd_card_mount() != ESP_OK)
            {
                ESP_LOGE(TAG_LOG, "Failed to mount SD card");
                _nextFWState = LOGGER_FW_ERROR;
                break;
            } 


            /* Start firmware upgrade */
            if (flash_stm32() != ESP_OK) {
                ESP_LOGE(TAG_LOG, "Support chip  failed!");
                _nextFWState = LOGGER_FW_ERROR;
            } else {
                ESP_LOGI(TAG_LOG, "Support chip flashed (2 / 6)");
                _nextFWState = LOGGER_FW_FLASHING_WWW;
            }

            break;

            case LOGGER_FW_FLASHING_WWW:

                if (update_www() != ESP_OK) {
                    ESP_LOGE(TAG_LOG, "File system flash failed");
                    _nextFWState = LOGGER_FW_ERROR;
                } else {
                    ESP_LOGI(TAG_LOG, "File system flashed (4 / 6)");
                    
                    _nextFWState = LOGGER_FW_FLASHING_ESP;
                }
            break;

        
            
            case LOGGER_FW_FLASHING_ESP:
                if (updateESP32() != ESP_OK) {
                    ESP_LOGE(TAG_LOG, "Main chip flash failed");
                    _nextFWState = LOGGER_FW_ERROR;
                } else {
                    ESP_LOGI(TAG_LOG, "Main flash chip flashed (6 / 6)");
                    _nextFWState = LOGGER_FW_DONE;
                }
            break;

            case LOGGER_FW_ERROR:
            case LOGGER_FW_DONE:
                esp_sd_card_unmount();

                settings_clear_bootreason();
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                esp_restart();
            break;
            // return ESP_OK;

            

            // esp_sd_card_unmount();
            // return;

        } // end of case 

        // If nextFWState is not NULL, then we have to send it to the queue
        if (_nextFWState != LOGGER_FW_EMPTY_STATE) xQueueSend(xQueueFW, &_nextFWState, 0);
        _nextFWState = LOGGER_FW_EMPTY_STATE;
        
    }

}


void task_logging(void * pvParameters)
{

    // esp_err_t ret;
    CLEAR_ERRORS(_errorCode);
   
    // uint32_t lastTick = 0;
    // uint32_t startTick = 0, stopTick = 0;

    uint8_t x = 0;

    // esp_err_t ret;
    // Init STM32 ADC enable pin
    // gpio_set_direction(GPIO_DATA_RDY_PIN, GPIO_MODE_INPUT);

    // spi_cmd_t spi_cmd;

    // ****************************
    // Async mem copy settings
    async_memcpy_config_t config = ASYNC_MEMCPY_DEFAULT_CONFIG();
    copy_done_sem =  xSemaphoreCreateBinary();
    // update the maximum data stream supported by underlying DMA engine
    config.backlog = 16;
    ESP_ERROR_CHECK(esp_async_memcpy_install(&config, &driver)); // install driver, return driver handle
    // End of async mem copy settings
    // ****************************
    
    sdcard_semaphore = xSemaphoreCreateBinary();
    idle_state = xSemaphoreCreateBinary();
    // immediately give semaphore
    xSemaphoreGive(sdcard_semaphore);
    xSemaphoreGive(idle_state);

    gpio_set_direction(GPIO_START_STOP_BUTTON, GPIO_MODE_INPUT);
    
    gpio_config_t adc_en_conf={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1<<GPIO_ADC_EN)
    };

    gpio_config(&adc_en_conf);
    gpio_set_level(GPIO_ADC_EN, 0);

    gpio_set_direction(GPIO_STM32_BOOT0, GPIO_MODE_OUTPUT);
    
    gpio_set_direction(SDCARD_POWER_EN, GPIO_MODE_OUTPUT);
    // boot STM32 normally
    gpio_set_level(GPIO_STM32_BOOT0, 0);
    // Enable power of sd card
    gpio_set_level(SDCARD_POWER_EN, 0);
    // Sysinfo init
    sysinfo_init();

    vTaskDelay(100 / portTICK_PERIOD_MS);

    gpio_config_t nreset_conf={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1<<GPIO_STM32_NRESET),
        .pull_up_en=GPIO_PULLUP_DISABLE
    };

    gpio_config(&nreset_conf);


    gpio_set_direction(SDCARD_CD, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SDCARD_CD, GPIO_PULLDOWN_ENABLE);
       
    // Initialize SD card

    if (esp_sd_card_check_for_card() == ESP_OK  &&
        esp_sd_card_mount() == ESP_OK)
    {
        #ifdef DEBUG_LOGTASK
        // ESP_LOGI(TAG_LOG, "File seq nr: %d", fileman_search_last_sequence_file());
        // ESP_LOGI(TAG_LOG, "File prefix: %s", "test");
        #endif
        Logger_check_sdcard_free_space();
        
        // esp_sd_card_unmount();
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

    // Reset the STM32
    Logger_resetSTM32();

     if (Logger_syncSettings(0) != ESP_OK)
        {
            ESP_LOGE(TAG_LOG, "STM32 settings FAILED");
        } else {
            #ifdef DEBUG_LOGTASK
            ESP_LOGI(TAG_LOG, "STM32 settings synced");
            #endif
        }

    spi_msg_slow_freq_1_ptr = (spi_msg_1_t *)spi_buffer;
    spi_msg_slow_freq_2_ptr = (spi_msg_2_t *)spi_buffer;

   // Create queue for tasks
    xQueue = xQueueCreate( 10, sizeof( LoggerState_t ) );
    xQueueFW = xQueueCreate( 10, sizeof( LoggerFWState_t ) );

    if (settings_get_boot_reason() == 1)
    {
        settings_clear_bootreason();
        LoggingState_t t = LOGTASK_FWUPDATE;
        LoggerFWState_t t2 = LOGGER_FW_FLASHING_STM;
        xQueueSend(xQueue, &t, 0);
        xQueueSend(xQueueFW, &t2, 0);
    } 

    while(1) {

        // Check if SDCARD is available or not 
        Logging_check_sdcard();

        // Wait for infinite time to do something
        if (xQueueReceive(xQueue, &_currentLogTaskState, 200 / portTICK_PERIOD_MS) != pdTRUE)
        {
            x++;
            if (x == 2)
            {
                LoggerState_t t = LOGTASK_SINGLE_SHOT;
                xQueueSend(xQueue, &t, 0);
                x = 0;
            }
            
            _currentLogTaskState = LOGTASK_IDLE;
            
        } else {
            ESP_LOGI(TAG_LOG, "Received task: %d", _currentLogTaskState);
        }
            
        switch (_currentLogTaskState)
        {
            case LOGTASK_IDLE:   /* not-a-thing, noppa, nada */                 break;
            case LOGTASK_CALIBRATION:       Logtask_calibration();              break;
            case LOGTASK_SINGLE_SHOT:       Logtask_singleShot();               break;
            case LOGTASK_PERSIST_SETTINGS:  settings_persist_settings();        break;
            case LOGTASK_SYNC_SETTINGS:     Logger_syncSettings(0);             break;
            case LOGTASK_SYNC_TIME:         Logger_syncSettings(1);             break;
            case LOGTASK_LOGGING:           Logtask_logging();                  break;
            case LOGTASK_REBOOT_SYSTEM:
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                esp_restart();
            break;
            case LOGTASK_FWUPDATE:          Logtask_fw_update();                break;
            case LOGTASK_FORMAT_SDCARD:     esp_sd_card_format();               break;
            // case LOGTASK_WIFI_CONNECT_AP:   wifi_connect_to_ap();               break;
            // case LOGTASK_WIFI_DISCONNECT_AP: wifi_disconnect_ap();              break;

            default:                                                    
            ESP_LOGE(TAG_LOG, "Unknown task: %d", _currentLogTaskState);        break;
        }// end of switch
        
      

    } // end of while(1)
     
    
}

