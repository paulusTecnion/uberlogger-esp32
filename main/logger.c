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
#include "extimer.h"
#include "fileman.h"

#define SPI_BUFFERSIZE_TX 16
#define SPI_BUFFERSIZE_RX 8192
#define STM_TXLENGTH 1024
// #define SD_BUFFERSIZE 8192

static const char* TAG_LOG = "LOGGER";

DMA_ATTR uint8_t sendbuf[SPI_BUFFERSIZE_TX];
// Two buffers for receiving
DMA_ATTR uint8_t recvbuf0[SPI_BUFFERSIZE_RX];
DMA_ATTR uint8_t recvbuf1[SPI_BUFFERSIZE_RX];



// tbuffer and tbuffer_i32 should be merged in the future
// uint8_t tbuffer[SD_BUFFERSIZE];

// Same size as SPI buffer but then int32 size
int32_t tbuffer_i32[SPI_BUFFERSIZE_RX];
char strbuffer[16];
LoggerState_t _currentLoggerState = LOGGER_IDLE;
LoggerState_t _nextLoggerState = LOGGER_IDLE;
LoggingState_t _currentLoggingState = LOGGING_STOP;
LoggingState_t _nextLoggingState = LOGGING_STOP;

uint32_t ulNotificationValue = 0;
uint32_t writeptr = 0;
int64_t t0, t1,t2,t3;
spi_device_handle_t handle;
spi_transaction_t _spi_transaction_rx0, _spi_transaction_rx1;
const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 100 );
extern TaskHandle_t xHandle_stm32;
bool int_level = 0;
// const UBaseType_t xArrayIndex = 0;

uint8_t volatile int_counter =0;

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

    //Give the semaphore.
    // BaseType_t mustYield = false;
    // xSemaphoreGiveFromISR(rdySem, &mustYield);
    // if (mustYield) {
    //     portYIELD_FROM_ISR();
    // }
    
    // int_level inits at 0. So first trigger it will become 1 and then 0 again etc. 
    int_level = !int_level;

    if (int_level) int_counter++;
    
    vTaskNotifyGiveFromISR( xHandle_stm32,
                                //    xArrayIndex,
                                   &xYieldRequired );
    
    portYIELD_FROM_ISR(xYieldRequired);
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

LoggerState_t LoggerGetState()
{
    return _currentLoggerState;
}

uint8_t Logger_enterSettingsMode()
{
    // Typical usage, go to settings mode, set settings, sync settings, exit settings mode
    if (_currentLoggerState == LOGGER_IDLE)
    {
        _nextLoggerState = LOGGER_SETTINGS;
        return RET_OK;
    } else {
        return RET_NOK;
    }
}

uint8_t Logger_exitSettingsMode()
{
     // Typical usage, go to settings mode, set settings, sync settings, exit settings mode
    if (_currentLoggerState == LOGGER_SETTINGS)
    {
        if (Logger_syncSettings() == RET_OK)
        {
            _nextLoggerState = LOGGER_IDLE;
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
    if (_currentLoggerState == LOGGER_LOGGING)
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
    _spi_transaction_rx0.length = 16; // in bits!
    _spi_transaction_rx0.rxlength = 16;
    _spi_transaction_rx0.rx_buffer = recvbuf0;
    _spi_transaction_rx0.tx_buffer = sendbuf;
    //   ESP_LOGI(TAG_LOG,"About to send the command in 1 second");
    
    
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
    

    // Tx length = 0
    // trans->length = 1;
    // Rx length = 1
    // trans->rxlength=1;
    // spi_device_transmit(handle, trans);
    // while(gpio_get_level(GPIO_DATA_RDY_PIN));

    // vTaskDelay( 20 / portTICK_PERIOD_MS);
    vTaskDelay( 10 / portTICK_PERIOD_MS);
    sendbuf[0] = STM32_CMD_NOP;
    sendbuf[1] = 0;
    _spi_transaction_rx0.rxlength = 16;
    _spi_transaction_rx0.rx_buffer = recvbuf0;
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

uint8_t Logger_syncSettings()
{
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

uint8_t Logger_start()
{
    if (_currentLoggerState == LOGGER_IDLE)
    {
        // gpio_set_level(GPIO_ADC_EN, 1);
        _nextLoggerState = LOGGER_LOGGING;
        return RET_OK;
    } 
    else 
    {
        return RET_NOK;
    }
}

uint8_t Logger_stop()
{
    if (_currentLoggerState == LOGGER_LOGGING)
    {
        // Disable interrupt data ready pin
        // gpio_set_level(GPIO_ADC_EN, 0);
        _nextLoggerState = LOGGER_IDLE;
        ESP_LOGI(TAG_LOG, "Logger_stop() called");
        return RET_OK;
    } 
    else 
    {
        return RET_NOK;
    }
}

uint8_t Logger_flush_buffer_to_sd_card_uint8(uint8_t * buffer, size_t size)
{
    // ESP_LOGI(TAG_LOG, "Flusing buffer to SD card");

    // uint16_t writeSize = 0, writeOffset=0;
    // if(writeptr > SD_BUFFERSIZE /2)
    // {
    //     writeSize = SD_BUFFERSIZE - writeptr;
    //     writeOffset = SD_BUFFERSIZE / 2;
    // } else {
    //     writeSize = writeptr;
    // }

    // ESP_LOGI(TAG_LOG, "WriteSize %d, writeOffset %d", writeSize, writeOffset);
    // ESP_LOGI(TAG_LOG, "Size: %d", size);

    
    fileman_write(buffer, size);
       
    return RET_OK;
   
}

uint8_t Logger_flush_buffer_to_sd_card_int32(int32_t * buffer, size_t size)
{
    ESP_LOGI(TAG_LOG, "Flusing CSV buffer to SD card");
    ESP_LOGI(TAG_LOG, "Size: %d", size);
    fileman_csv_write(buffer, size);

    return RET_OK;
}

uint8_t Logger_raw_to_csv(uint8_t * buffer, size_t size, uint8_t log_counter)
{
     // ESP_LOGI(TAG_LOG,"ADC Reading:");
        writeptr = 0;
        ESP_LOGI(TAG_LOG, "raw_to_csv log_counter %d", log_counter);
        for (int j = 0; j < (size); j = j + 2)
        {
            // we'll have to multiply this with 20V/4096 = 0.00488281 V per LSB
            // Or in fixed point Q6.26 notation 488281 = 1 LSB
            
            // What we want to achieve here is 20V/4095 - 10V, but then in fixed point notation. 
            // To achieve that, we will multiply the numbers by 1000000.
            // Then, instead of dividing the number by 4095 or use 0.0488281, we divide by the byte shift of 1<<12 which is more accurate with int32_t. 
            
            // Next steps can be merged, but are now seperated for checking values
            // First shift the bytes to get the ADC value
            t0 = ((int32_t)buffer[j] | ((int32_t)buffer[j + 1] << 8));
            t1 = t0 * (20LL * 1000000LL);
            t2 = t1 / ((1 << settings_get_resolution_uint8()) - 1); // -1 for 4095 steps
            t3 = t2 - 10000000LL;
            // In one buffer of STM_TXLENGTH bytes, there are only STM_TXLENGTH/2 16 bit ADC values. So divide by 2
            tbuffer_i32[writeptr+(log_counter*(STM_TXLENGTH/2))] = (int32_t)t3;
            
            ESP_LOGI(TAG_LOG, "%d, %d, %lld, %d", buffer[j], buffer[j+1], t3, tbuffer_i32[writeptr+(log_counter)*(STM_TXLENGTH/2)]);
            writeptr++;
        }

        return RET_OK;
}

void Logger_log()
{   
    
    static uint8_t log_counter = 0;
    static bool buffer_no = false;
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
        }  else {
            // _currentLoggingState = LOGGING_RX0_WAIT;
            // _currentLoggingState = LOGGING_STOP;
            // ESP_LOGI(TAG_LOG, "No DATA RDY intterupt");
        }

        switch (_currentLoggingState)
        {
            case LOGGING_RX0_WAIT:
                {
                        // Check if our transaction is done
                        spi_transaction_t * ptr = &_spi_transaction_rx0;
                        if(spi_device_get_trans_result(handle, &ptr, portMAX_DELAY) == ESP_OK)
                        {
                            // ESP_LOGI(TAG_LOG, "log_counter:%d", log_counter);
                            // for (int i=0; i<16; i = i + 2)
                            // {
                            //     ESP_LOGI(TAG_LOG, "%d, %d", recvbuf0[(log_counter*STM_TXLENGTH+i)], recvbuf0[(log_counter*STM_TXLENGTH)+i+1]);

                            // }
                            
                            
                            // one block of 512 bytes is retrieved, increase message count
                          
                            // ESP_LOGI(TAG_LOG, "%d vs. %d", log_counter, (int_counter-1));
                            if (log_counter != (int_counter-1))
                            {

                                ESP_LOGE(TAG_LOG, "Missing SPI transaction (%d vs. %d)! Stopping", log_counter, (int_counter));
                                // Logger_stop();
                                // return;
                            }

                            // execute only when a full block of SPI_BUFFERSIZE_RX is retrieved
                            // Process data first
                            if (settings_get_logmode() == LOGMODE_CSV)
                            {
                                // if(buffer_no==false){
                                    Logger_raw_to_csv(recvbuf0+(log_counter*STM_TXLENGTH), STM_TXLENGTH, log_counter);
                                // }else if(buffer_no==true){
                                    // Logger_raw_to_csv(recvbuf1+(log_counter*STM_TXLENGTH), STM_TXLENGTH, log_counter);
                                // }
                            }

                            log_counter++; // received bytes = log_counter*512

                            if(log_counter*STM_TXLENGTH >= SPI_BUFFERSIZE_RX){
                                log_counter = 0;
                                int_counter = 0;
                                if (settings_get_logmode() == LOGMODE_CSV)
                                {
                                    // Write it SD
                                    Logger_flush_buffer_to_sd_card_int32(tbuffer_i32, SPI_BUFFERSIZE_RX);
                                } else {
                                    Logger_flush_buffer_to_sd_card_uint8(recvbuf0, SPI_BUFFERSIZE_RX);
                                }                    

                                buffer_no = !buffer_no;
                            }

                            _nextLoggingState = LOGGING_STOP;
                        } else {
                            ESP_LOGE(TAG_LOG, "RX0 timed out!");
                            _nextLoggingState = LOGGING_STOP;
                            return;
                        }
                        _nextLoggingState = LOGGING_STOP;
                }
                break;

            case LOGGING_START:
                    // ESP_LOGI(TAG_LOG, "Queuing spi transactions..");
                    // assert(spi_device_transmit(handle, &_spi_transaction) == ESP_OK);
                    if (int_counter==0)
                    {
                        count_offset = 0;
                    } 
                    _spi_transaction_rx0.length = STM_TXLENGTH*8;
                    _spi_transaction_rx0.rxlength=STM_TXLENGTH*8;
                    _spi_transaction_rx0.tx_buffer = NULL;
                    // 
                    // if(buffer_no==false){
                        _spi_transaction_rx0.rx_buffer=recvbuf0+(log_counter*STM_TXLENGTH);
                    // }else if(buffer_no == true){
                        // _spi_transaction_rx0.rx_buffer=recvbuf1+(log_counter*STM_TXLENGTH);
                    // }
                    ESP_LOGI(TAG_LOG, "Queuing SPI trans");
                    assert(spi_device_queue_trans(handle, &_spi_transaction_rx0, 0) == ESP_OK);

                    _nextLoggingState = LOGGING_RX0_WAIT;
                    

            break;


            case LOGGING_STOP:
                
            break;
        }

       
       

        if (_nextLoggingState != _currentLoggingState)
        {
            
            ESP_LOGI(TAG_LOG, "LOGGING state changing from %d to %d", _currentLoggingState, _nextLoggingState);
            _currentLoggingState = _nextLoggingState;
        }
                
    
    gpio_set_level(GPIO_CS, 0);
    
    
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

    // //Initialize the SPI bus and add the device we want to send stuff to.
    ret=spi_bus_initialize(SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(SENDER_HOST, &devcfg, &handle);
    assert(ret==ESP_OK);
    // // take the bus and never let go :-)
    ret = spi_device_acquire_bus(handle, portMAX_DELAY);
    assert(ret==ESP_OK);

    // // Initialize SD card
    esp_sd_card_init();
    esp_sd_card_mount();
    // // need to check if sdcard is mounted
    ESP_LOGI(TAG_LOG, "File seq nr: %d", fileman_search_last_sequence_file());
    esp_sd_card_unmount();

    ESP_LOGI(TAG_LOG, "Logger task started");
    if (Logger_syncSettings() )
    {
        ESP_LOGE(TAG_LOG, "STM32 settings FAILED");
    } else {
        ESP_LOGI(TAG_LOG, "STM32 settings synced");
    }
    
    while(1) {
       

        switch (_currentLoggerState)
        {
            case LOGGER_IDLE:
                vTaskDelay(500 / portTICK_PERIOD_MS);
                
                if (_nextLoggerState == LOGGER_LOGGING)
                {
                    esp_sd_card_mount();
                    if (fileman_open_file() != ESP_OK)
                    { 
                        esp_sd_card_unmount();
                        _nextLoggerState = LOGGER_IDLE;
                        break;
                    }
                    _nextLoggingState = LOGGING_STOP;
                    // upon changing state to logging, make sure these settings are correct. 
                    // _spi_transaction_rx0.length=sizeof(sendbuf)*8; // size in bits
                    _spi_transaction_rx0.length=STM_TXLENGTH*8; // size in bits
                    _spi_transaction_rx0.rxlength = STM_TXLENGTH*8; // size in bits
                    // _spi_transaction.tx_buffer=sendbuf;
                    _spi_transaction_rx0.rx_buffer=recvbuf0;
                    _spi_transaction_rx0.tx_buffer=NULL;

                    // _spi_transaction_rx1.length=sizeof(sendbuf)*8; // size in bits
                    _spi_transaction_rx1.length = STM_TXLENGTH*8; // size in bits
                    _spi_transaction_rx1.rxlength = STM_TXLENGTH*8; // size in bits
                    // _spi_transaction.tx_buffer=sendbuf;
                    _spi_transaction_rx1.rx_buffer=recvbuf1;
                    _spi_transaction_rx1.tx_buffer=NULL;
                    // writeptr = 0;
                    _nextLoggingState = LOGGING_STOP;
                    // esp_sd_card_mount_open_file();
                    // enable data_rdy interrupt
                    assert(Logger_datardy_int(1) == RET_OK);
                    // Enable logging
                    gpio_set_level(GPIO_ADC_EN, 1);
                }
            break;

            case LOGGER_LOGGING:
                
                Logger_log();
                if (_nextLoggerState == LOGGER_IDLE)
                {
                    // Disable logging (should change this)
                    gpio_set_level(GPIO_ADC_EN, 0);
                    // disable data_rdy interrupt
                    assert(Logger_datardy_int(0) == RET_OK);
                    // Flush buffer to sd card
                    if (settings_get_logmode() == LOGMODE_CSV)
                    {
                        Logger_flush_buffer_to_sd_card_int32(tbuffer_i32, SPI_BUFFERSIZE_RX);
                    } else {
                        Logger_flush_buffer_to_sd_card_uint8(recvbuf0, SPI_BUFFERSIZE_RX);
                    }
                    
                    fileman_close_file();
                    esp_sd_card_unmount();
                    
                }

            break;

            case LOGGER_SETTINGS:

            
            break;


            default:

            break;
            // should not come here
        }

        if (_nextLoggerState != _currentLoggerState)
        {
            ESP_LOGI(TAG_LOG, "Changing LOGGER state from %d to %d", _currentLoggerState, _nextLoggerState);
            _currentLoggerState = _nextLoggerState;
        }


    }
     
    
}

