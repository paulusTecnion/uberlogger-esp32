#include "logger.h"
#include <stdio.h>
#include "common.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "hw_config.h"
#include "esp_sd_card.h"
#include "driver/spi_master.h"
#include "settings.h"
#include "fileman.h"
#include "tempsensor.h"
#include "time.h"

// Buffer size when sending data to STM. Value in bytes
#define STM_SPI_BUFFERSIZE_CMD_TX 2
// Receiving buffersize when in configuration mode. Value in bytes
#define STM_SPI_BUFFERSIZE_CMD_RX 2
// One line of the spi buffer depends on at what stage of sending it is. For the first half of the ADC conversion we have:
// [start bytes][39*(1 year byte, 1 month byte, 1 date byte, 1 hour, 1 second byte, 4 subsecondsTime bytes) Time bytes][60*GPIO bytes][60*8channels*2 ADC bytes]
// For the second half we have :
// [39*8channels*2 ADC bytes][39*GPIO bytes][39*(1 year byte, 1 month byte, 1 date byte, 1 hour, 1 second byte, 4 subsecondsTime bytes) Time bytes][Stop bytes]
// So the SPI TX length is: 
// 2 start/stop bytes   = 2
// 39*8*2 ADC           = 624
// 39 * 1 GPIO          = 39
// 39*(1+1+1+1+1+1+4)Time= 351
//  Total               = 1016 bytes



spi_msg_1_t * spi_msg_1_ptr;
spi_msg_2_t * spi_msg_2_ptr;

uint8_t msg_part = 0;

// Buffer for sending data to the STM
DMA_ATTR uint8_t sendbuf[STM_SPI_BUFFERSIZE_CMD_TX];
// Buffer for receiving data from the STM
DMA_ATTR uint8_t recvbuf0[sizeof(spi_msg_1_t)];


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

// handle to spi device
spi_device_handle_t handle;
// transactions variables for doing spi transactions with stm
spi_transaction_t _spi_transaction_rx0, _spi_transaction_rx1;

// max block time for interrupt
const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 100 );
// Handle to stm32 task
extern TaskHandle_t xHandle_stm32;
// Interrupt notification value
uint32_t ulNotificationValue = 0;

// State of STM interrupt pin. 0 = low, 1 = high
bool int_level = 0;
// Interrupt counter that tracks how many times the interrupt has been triggered. 
uint8_t volatile int_counter =0;

  static uint8_t log_counter = 0;
/*
This ISR is called when the handshake line goes high OR low
*/
static void IRAM_ATTR gpio_handshake_isr_handler(void* arg)
{
     BaseType_t xYieldRequired = pdFALSE;

     
    //Sometimes due to interference or ringing or something, we get two irqs after eachother. This is solved by
    //looking at the time between interrupts and refusing any interrupt too close to another one.
    // static uint32_t lasthandshaketime_us = 0;
    // uint32_t currtime_us = esp_timer_get_time();
    // uint32_t diff = currtime_us - lasthandshaketime_us;
    // // Limit till 200kHz
    // if (diff < 5) {
    //     return; 
    // }
    // lasthandshaketime_us = currtime_us;

    // int_level inits at 0. So first trigger it will become 1 and then 0 again etc. 
    int_level = !int_level;

    if (int_level) int_counter++;
    
    vTaskNotifyGiveFromISR( xHandle_stm32,
                                //    xArrayIndex,
                                   &xYieldRequired );
    
    portYIELD_FROM_ISR(xYieldRequired);
}

int32_t Logger_convertAdcFixedPoint(uint8_t adcData0, uint8_t adcData1)
{
    t0 = ((int32_t)adcData0 | ((int32_t)adcData1 << 8));
            
    // In one buffer of STM_TXLENGTH bytes, there are only STM_TXLENGTH/2 16 bit ADC values. So divide by 2
    t1 = t0 * (-20LL * 1000000LL); // note the minus for inverted input!
    t2 = t1 / ((1 << settings_get_resolution()) - 1); // -1 for 4095 steps
    t3 = t2 + 10000000LL;
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

uint8_t Logger_datardy_int(uint8_t value) 
{
    if (value == 1)
    {
        ESP_LOGI(TAG_LOG, "Enabling data_rdy interrupts");
        // Trigger on up and down edges
        if (gpio_set_intr_type(GPIO_DATA_RDY_PIN, GPIO_INTR_ANYEDGE) == ESP_OK &&
        gpio_install_isr_service(0) == ESP_OK &&
        gpio_isr_handler_add(GPIO_DATA_RDY_PIN, gpio_handshake_isr_handler, NULL) == ESP_OK){
            return RET_OK;
        } else {
            return RET_NOK;
        }
        
    } else if (value == 0) {
        ESP_LOGI(TAG_LOG, "Disabling data_rdy interrupts");
        gpio_isr_handler_remove(GPIO_DATA_RDY_PIN);
        gpio_uninstall_isr_service();
        return RET_OK;
    } else {
        return RET_NOK;
    }

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

void Logger_spi_cmd(stm32cmd_t cmd, uint8_t data)
{
    // uint8_t * ptr;
    // ptr = trans->tx_buffer;
    // ptr[0] = cmd;
    // trans->rxlength = 1;
    // spi_device_transmit(handle, trans);
    sendbuf[0] = (uint8_t) cmd;
    sendbuf[1] = data;
    _spi_transaction_rx0.length = STM_SPI_BUFFERSIZE_CMD_TX*8; // in bits!
    _spi_transaction_rx0.rxlength = STM_SPI_BUFFERSIZE_CMD_RX*8;
    _spi_transaction_rx0.rx_buffer = recvbuf0;
    _spi_transaction_rx0.tx_buffer = sendbuf;
       

    // Wait until data ready pin is LOW
    while(gpio_get_level(GPIO_DATA_RDY_PIN));
    assert(spi_device_transmit(handle, &_spi_transaction_rx0) == ESP_OK);
    // assert(spi_device_polling_transmit(handle, &_spi_transaction) == ESP_OK);
    // wait for 5 ms for stm32 to process data
    // Wait until data is ready for transmission
    vTaskDelay( 10 / portTICK_PERIOD_MS);
    while(!gpio_get_level(GPIO_DATA_RDY_PIN));
    ESP_LOGI(TAG_LOG,"Pass 1/2 CMD");
    // ets_delay_us(10000);
    

    // vTaskDelay( 20 / portTICK_PERIOD_MS);
    vTaskDelay( 10 / portTICK_PERIOD_MS);

    sendbuf[0] = STM32_CMD_NOP;
    sendbuf[1] = 0;
    // _spi_transaction_rx0.rxlength = 16;
    // _spi_transaction_rx0.rx_buffer = recvbuf0;
    // Wait until data_rdy pin is low again, then data transmission is complete
    spi_device_transmit(handle, &_spi_transaction_rx0);
    while(gpio_get_level(GPIO_DATA_RDY_PIN));
    ESP_LOGI(TAG_LOG,"Pass 2/2 CMD");    

}

void Logger_print_rx_buffer(uint8_t * rxbuf)
{
    ESP_LOGI(TAG_LOG, "recvbuf:");
    for (int i=0; i<16;i++)
    {
        ESP_LOGI(TAG_LOG, "%d", rxbuf[i]);
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

uint8_t Logger_syncSettings()
{
    settings_persist_settings();
    // Send command to STM32 to go into settings mode
    ESP_LOGI(TAG_LOG, "Setting SETTINGS mode");
    Logger_spi_cmd(STM32_CMD_SETTINGS_MODE, 0);
    // Logger_print_rx_buffer();
    if (recvbuf0[0] != STM32_RESP_OK || recvbuf0[1] != STM32_CMD_SETTINGS_MODE)
    {
        ESP_LOGI(TAG_LOG, "Unable to put STM32 into SETTINGS mode. ");
        Logger_print_rx_buffer(recvbuf0);
        return RET_NOK;
    } 
    ESP_LOGI(TAG_LOG, "SETTINGS mode enabled");



    Logger_spi_cmd(STM32_CMD_SET_ADC_CHANNELS_ENABLED, settings_get_adc_channel_enabled_all());
    // Logger_print_rx_buffer();
    if (recvbuf0[0] != STM32_RESP_OK || recvbuf0[1] != STM32_CMD_SET_ADC_CHANNELS_ENABLED)
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 ADC channels. Received %d", recvbuf0[0]);
        Logger_print_rx_buffer(recvbuf0);
        return RET_NOK;
    }

    ESP_LOGI(TAG_LOG, "ADC channels set");

    Logger_spi_cmd(STM32_CMD_SET_RESOLUTION, (uint8_t)settings_get_resolution());
    // Logger_print_rx_buffer();
    if (recvbuf0[0] != STM32_RESP_OK || recvbuf0[1] != STM32_CMD_SET_RESOLUTION)
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 ADC resolution. Received %d", recvbuf0[0]);
        Logger_print_rx_buffer(recvbuf0);
        return RET_NOK;
    } 
    
    ESP_LOGI(TAG_LOG, "ADC resolution set");
    

    Logger_spi_cmd(STM32_CMD_SET_SAMPLE_RATE, (uint8_t)settings_get_samplerate());
    // Logger_print_rx_buffer();
    if (recvbuf0[0] != STM32_RESP_OK || recvbuf0[1] != STM32_CMD_SET_SAMPLE_RATE)
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 sample rate. ");
        Logger_print_rx_buffer(recvbuf0);
        return RET_NOK;
    }

    ESP_LOGI(TAG_LOG, "Sample rate set");

    // Send settings one by one and confirm
    Logger_spi_cmd(STM32_CMD_MEASURE_MODE, 0);
    // Logger_print_rx_buffer();
    if (recvbuf0[0] != STM32_RESP_OK || recvbuf0[1] != STM32_CMD_MEASURE_MODE)
    {
        ESP_LOGI(TAG_LOG, "Unable to set STM32 in measure mode");
        Logger_print_rx_buffer(recvbuf0);
        return RET_NOK;
    }
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

esp_err_t Logger_start()
{
    if (_currentLogTaskState == LOGTASK_IDLE)
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
        _nextLogTaskState = LOGTASK_IDLE;
       
        return ESP_OK;
    } 
    else 
    {
        return ESP_FAIL;
    }
}

uint8_t Logger_flush_buffer_to_sd_card_uint8(uint8_t * buffer, size_t size)
{
    ESP_LOGI(TAG_LOG, "Flusing buffer to SD card");
    
    fileman_write(buffer, size);
       
    return RET_OK;
   
}

uint8_t Logger_flush_buffer_to_sd_card_csv(int32_t * adcData, size_t lenAdc, uint8_t * gpioData, size_t lenGpio, uint8_t * timeData, size_t lenTime)
{
    ESP_LOGI(TAG_LOG, "Flusing CSV buffer to SD card");
    fileman_csv_write(adcData, lenAdc, gpioData, lenGpio, timeData ,lenTime);
    return RET_OK;
}

// uint8_t Logger_raw_to_csv(uint8_t * buffer, size_t size, uint8_t log_counter)
uint8_t Logger_raw_to_csv(uint8_t log_counter, const uint8_t * adcData, size_t length)
{
     // ESP_LOGI(TAG_LOG,"ADC Reading:");
        int j;
        uint32_t writeptr = 0;
        ESP_LOGI(TAG_LOG, "raw_to_csv log_counter %d", log_counter);
        for (j = 0; j < length; j = j + 2)
        {
            // we'll have to multiply this with 20V/4096 = 0.00488281 V per LSB
            // Or in fixed point Q6.26 notation 488281 = 1 LSB
            // Note: 4095 = -10 and 0 = 10V (theoratically, without input impedance)

            // What we want to achieve here is -20V/4095 + 10V, but then in fixed point notation. 
            // To achieve that, we will multiply the numbers by 1000000.
            // Then, instead of dividing the number by 4095 or use 0.0488281, we divide by the byte shift of 1<<12 which is more accurate with int32_t. 
            
            // Next steps can be merged, but are now seperated for checking values
            // First shift the bytes to get the ADC value
            
           
            adc_buffer_fixed_point[writeptr+(log_counter*(length/2))] = Logger_convertAdcFixedPoint(adcData[j], adcData[j+1]);
            

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

esp_err_t Logger_log()
{   
    
  
    
    static uint8_t count_offset = 1;

    ulNotificationValue = ulTaskNotifyTake( 
                                            // xArrayIndex,
                                            pdTRUE,
                                            xMaxBlockTime );

        if (ulNotificationValue)
        {
            if (int_level)
            {
                // ESP_LOGI(TAG_LOG, "HIGH TRIGGER");
                _nextLoggingState = LOGGING_START;
            }  else  {
                // ESP_LOGI(TAG_LOG, "LOW TRIGGER");
            }
        }  
        // else {
            // Throw error

            // _currentLoggingState = LOGGING_IDLE;

            // ESP_LOGI(TAG_LOG, "No DATA RDY intterupt");
        // }

        switch (_currentLoggingState)
        {
           
            case LOGGING_START:
                    // ESP_LOGI(TAG_LOG, "Queuing spi transactions..");
                    // assert(spi_device_transmit(handle, &_spi_transaction) == ESP_OK);
                    if (int_counter==0)
                    {
                        count_offset = 0;
                    } 
                    // There is no TX data, but TX length cannot be smaller than RX length in full duplex
                    _spi_transaction_rx0.length = sizeof(recvbuf0)*8;
                    _spi_transaction_rx0.rxlength=sizeof(recvbuf0)*8;
                    _spi_transaction_rx0.tx_buffer = NULL;                 
                    // _spi_transaction_rx0.rx_buffer=(uint8_t*)&recvbuf0+(log_counter*STM_TXLENGTH);
                    _spi_transaction_rx0.rx_buffer=(uint8_t*)&recvbuf0;

                    ESP_LOGI(TAG_LOG, "Queuing SPI trans");
                    assert(spi_device_queue_trans(handle, &_spi_transaction_rx0, 0) == ESP_OK);

                    _nextLoggingState = LOGGING_RX0_WAIT;
                    

            break;
            
            case LOGGING_RX0_WAIT:
                {
                        // Check if our transaction is done
                        spi_transaction_t * ptr = &_spi_transaction_rx0;
                        if(spi_device_get_trans_result(handle, &ptr, 1000 / portTICK_PERIOD_MS) == ESP_OK)
                        {
                            ESP_LOGI(TAG_LOG, "log_counter:%d", log_counter);
                            // for (int i=0; i<sizeof(recvbuf0)/4; i++)
                            // {
                            //     ESP_LOGI(TAG_LOG, "Byte %d, %d", i, (recvbuf0[i]));
                            // }

                            //  for (int i=sizeof(recvbuf0)*3/4; i<sizeof(recvbuf0); i++)
                            // {
                            //     ESP_LOGI(TAG_LOG, "Byte %d, %d", i, (recvbuf0[i]));
                            // }
                            
                            if (log_counter != (int_counter-1))
                            {

                                ESP_LOGE(TAG_LOG, "Missing SPI transaction (%d vs. %d)! Stopping", log_counter, (int_counter));
                                return ESP_FAIL;
                            }


                            // Copy values to buffer

                            // Check if the start bytes are first or last in the received SPI buffer
                           
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
                                    ESP_LOGE(TAG_LOG, "No start or stop byte found! Stop bytes: %d, %d", spi_msg_2_ptr->stopByte[0], spi_msg_2_ptr->stopByte[1]);
                                    
                                //     return ESP_FAIL;
                                }
                        
                            // Process data first
                            if (settings_get_logmode() == LOGMODE_CSV)
                            {
                                Logger_raw_to_csv(log_counter, sdcard_data.adcData+log_counter*sizeof(spi_msg_1_ptr->adcData), sizeof(spi_msg_1_ptr->adcData));
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
                            if(log_counter*sizeof(spi_msg_1_ptr->gpioData) >= sizeof(sdcard_data.gpioData)){
                                log_counter = 0;
                                int_counter = 0;
                                if (settings_get_logmode() == LOGMODE_CSV)
                                {
                                    // Write it SD
                                    ESP_LOGI(TAG_LOG, "ADC fixed p %d, %d, %d", adc_buffer_fixed_point[0], adc_buffer_fixed_point[1], adc_buffer_fixed_point[2]);
                                    ESP_LOGI(TAG_LOG, "GPIO %d, %d, %d", sdcard_data.gpioData[0], sdcard_data.gpioData[1], sdcard_data.gpioData[2]);
                                    ESP_LOGI(TAG_LOG, "Sizes: %d, %d, %d", sizeof(adc_buffer_fixed_point), sizeof(sdcard_data.gpioData), sizeof(sdcard_data.timeData));
                                    
                                    
                                    Logger_flush_buffer_to_sd_card_csv(
                                        adc_buffer_fixed_point, (sizeof(adc_buffer_fixed_point)/sizeof(int32_t)),
                                        sdcard_data.gpioData, sizeof(sdcard_data.gpioData), 
                                        sdcard_data.timeData, (sizeof(sdcard_data.timeData)/sizeof(s_date_time_t)) );
                                } else {
                                    Logger_flush_buffer_to_sd_card_uint8((uint8_t*)&sdcard_data, sizeof(sdcard_data));
                                }                    
                                
                            }

                            _nextLoggingState = LOGGING_IDLE;
                        } else {
                            ESP_LOGE(TAG_LOG, "RX0 timed out!");
                            _nextLoggingState = LOGGING_IDLE;
                            return ESP_ERR_TIMEOUT;
                        }
                        _nextLoggingState = LOGGING_IDLE;
                }
                break;


            case LOGGING_IDLE:
                
            break;
        }

       
       

        if (_nextLoggingState != _currentLoggingState)
        {
            
            ESP_LOGI(TAG_LOG, "LOGGING state changing from %d to %d", _currentLoggingState, _nextLoggingState);
            _currentLoggingState = _nextLoggingState;
        }
                
    
    gpio_set_level(GPIO_CS, 0);

    return ESP_OK;
    
    
}


void task_logging(void * pvParameters)
{
    

    esp_err_t ret;
    //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_MOSI,
        .miso_io_num=GPIO_MISO,
        .sclk_io_num=GPIO_SCLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz = 8192,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = ESP_INTR_FLAG_IRAM
    };

    //Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .clock_speed_hz=SPI_STM32_BUS_FREQUENCY, //400000,
        .duty_cycle_pos=128,        //50% duty cycle
        .mode=0,
        .spics_io_num=-1,//GPIO_CS,
        .cs_ena_posttrans=0,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=3,
        .flags = 0,
        .input_delay_ns=10
    };  

    gpio_config_t adc_en_conf={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1<<GPIO_ADC_EN)
    };

    

    //GPIO config for the handshake line.
    gpio_config_t io_conf={
        // .intr_type=GPIO_INTR_POSEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=0,
        .pin_bit_mask=(1<<GPIO_DATA_RDY_PIN)
    };

    // Init STM32 ADC enable pin
    gpio_set_direction(GPIO_DATA_RDY_PIN, GPIO_MODE_INPUT);
    
    ret = gpio_config(&adc_en_conf);
    gpio_set_level(GPIO_ADC_EN, 0);

    gpio_set_direction(GPIO_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_CS, 0);
  

    memset(&_spi_transaction_rx0, 0, sizeof(_spi_transaction_rx0));
    memset(&_spi_transaction_rx1, 0, sizeof(_spi_transaction_rx1));
    memset(recvbuf0, 0 , sizeof(recvbuf0));


    // //Initialize the SPI bus and add the device we want to send stuff to.
    ret=spi_bus_initialize(SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(SENDER_HOST, &devcfg, &handle);
    assert(ret==ESP_OK);
    // // take the bus and never let go :-)
    ret = spi_device_acquire_bus(handle, portMAX_DELAY);
    assert(ret==ESP_OK);

    // // Initialize SD card
    if (esp_sd_card_mount() == ESP_OK)
    {
        ESP_LOGI(TAG_LOG, "File seq nr: %d", fileman_search_last_sequence_file());
        esp_sd_card_unmount();
    } 
    

    ESP_LOGI(TAG_LOG, "Logger task started");
    if (Logger_syncSettings() )
    {
        ESP_LOGE(TAG_LOG, "STM32 settings FAILED");
    } else {
        ESP_LOGI(TAG_LOG, "STM32 settings synced");
    }
    
    spi_msg_1_ptr = (spi_msg_1_t*)recvbuf0;
    spi_msg_2_ptr = (spi_msg_2_t*)recvbuf0;


    while(1) {
       

        switch (_currentLogTaskState)
        {
            case LOGTASK_IDLE:
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
                                _nextLogTaskState = LOGTASK_IDLE;
                            }

                            break;
                        }
                        fileman_csv_write_header();
                        _nextLoggingState = LOGGING_IDLE;
                        // upon changing state to logging, make sure these settings are correct. 
                        // _spi_transaction_rx0.length=sizeof(sendbuf)*8; // size in bits
                        _spi_transaction_rx0.length=STM_SPI_BUFFERSIZE_DATA_RX*8; // size in bits
                        _spi_transaction_rx0.rxlength = STM_SPI_BUFFERSIZE_DATA_RX*8; // size in bits
                        // _spi_transaction.tx_buffer=sendbuf;
                        _spi_transaction_rx0.rx_buffer=(uint8_t*)&recvbuf0;
                        _spi_transaction_rx0.tx_buffer=NULL;

                        _nextLoggingState = LOGGING_IDLE;
                        // esp_sd_card_mount_open_file();
                        // enable data_rdy interrupt
                        assert(Logger_datardy_int(1) == RET_OK);
                        // Enable logging
                        gpio_set_level(GPIO_ADC_EN, 1);
                    } else {
                          _nextLogTaskState = LOGTASK_IDLE;
                    }
                   
                   
                }
            break;

            case LOGTASK_LOGGING:
                
                if (Logger_log() != ESP_OK)
                {
                    Logger_stop();
                }

                if (_nextLogTaskState == LOGTASK_IDLE)
                {
                    // Disable logging (should change this)
                    gpio_set_level(GPIO_ADC_EN, 0);
                     // disable data_rdy interrupt
                    assert(Logger_datardy_int(0) == RET_OK);


                    // Flush buffer to sd card
                    if (settings_get_logmode() == LOGMODE_CSV)
                    {
                        ESP_LOGI(TAG_LOG, "ADC fixed p %d, %d, %d", adc_buffer_fixed_point[0], adc_buffer_fixed_point[1], adc_buffer_fixed_point[2]);
                                    ESP_LOGI(TAG_LOG, "GPIO %d, %d, %d", sdcard_data.gpioData[0], sdcard_data.gpioData[1], sdcard_data.gpioData[2]);
                                    ESP_LOGI(TAG_LOG, "Sizes: %d, %d, %d", sizeof(adc_buffer_fixed_point), sizeof(sdcard_data.gpioData), sizeof(sdcard_data.timeData));
                                    
                         Logger_flush_buffer_to_sd_card_csv(
                                        adc_buffer_fixed_point, (sizeof(adc_buffer_fixed_point)/sizeof(int32_t)),
                                        sdcard_data.gpioData, sizeof(sdcard_data.gpioData), 
                                        sdcard_data.timeData, (sizeof(sdcard_data.timeData)/sizeof(s_date_time_t)));
                    } else {
                        Logger_flush_buffer_to_sd_card_uint8((uint8_t*)&recvbuf0, SD_BUFFERSIZE);
                    }
                    // reset counters
                    log_counter = 0;
                    int_counter = 0;
                    fileman_close_file();
                    esp_sd_card_unmount();
                    
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

