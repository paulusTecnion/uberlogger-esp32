#include "logger.h"
#include <stdio.h>
#include "common.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "hw_config.h"
#include "esp_sd_card.h"
#include "driver/spi_master.h"
#include "settings.h"

#define SPI_BUFFERSIZE 16
#define SD_BUFFERSIZE 8192


DMA_ATTR uint8_t sendbuf[SPI_BUFFERSIZE];
DMA_ATTR uint8_t recvbuf[SPI_BUFFERSIZE];
// tbuffer and tbuffer_i32 may be merged in the future
uint8_t tbuffer[SD_BUFFERSIZE];
int32_t tbuffer_i32[SD_BUFFERSIZE];
char strbuffer[16];
LoggerState _currentLoggerState = LOGGER_IDLE;
LoggerState _nextLoggerState = LOGGER_IDLE;
// uint8_t _logCsv = 1;
uint32_t ulNotificationValue;
uint32_t writeptr = 0;
int64_t t0, t1,t2,t3;
spi_device_handle_t handle;
spi_transaction_t _spi_transaction;
const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
extern TaskHandle_t xHandle_stm32;

/*
This ISR is called when the handshake line goes high.
*/
static void IRAM_ATTR gpio_handshake_isr_handler(void* arg)
{
     BaseType_t xYieldRequired = pdFALSE;

     
    //Sometimes due to interference or ringing or something, we get two irqs after eachother. This is solved by
    //looking at the time between interrupts and refusing any interrupt too close to another one.
    // static uint32_t lasthandshaketime_us;
    // uint32_t currtime_us = esp_timer_get_time();
    // uint32_t diff = currtime_us - lasthandshaketime_us;
    // if (diff < 1000) {
    //     return; //ignore everything <1ms after an earlier irq
    // }
    // lasthandshaketime_us = currtime_us;

    //Give the semaphore.
    // BaseType_t mustYield = false;
    // xSemaphoreGiveFromISR(rdySem, &mustYield);
    // if (mustYield) {
    //     portYIELD_FROM_ISR();
    // }
      vTaskNotifyGiveFromISR( xHandle_stm32,
                                //    xArrayIndex,
                                   &xYieldRequired );
    
    portYIELD_FROM_ISR(xYieldRequired);
}

LoggerState LoggerGetState()
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

void Logger_spi_cmd(stm32cmd_t cmd)
{
    memset(sendbuf, (uint8_t)cmd, 1);
    spi_device_transmit(handle, &_spi_transaction);
    // wait for 5 ms for stm32 to process data
    ets_delay_us(5000000);
    memset(sendbuf, 0, 1);
    spi_device_transmit(handle, &_spi_transaction);
}

uint8_t Logger_syncSettings()
{
    // Send command to STM32 to go into settings mode
    _spi_transaction.length=sizeof(sendbuf)*8; //sizeof(sendbuf)*8; // size in bits
    _spi_transaction.rxlength = sizeof(recvbuf)*8; //sizeof(recvbuf)*8; // size in bits
    _spi_transaction.tx_buffer=sendbuf;
    _spi_transaction.rx_buffer=recvbuf;
    _spi_transaction.tx_buffer=NULL;
    _spi_transaction.rx_buffer=recvbuf;
    ESP_LOGI(TAG, "Setting SETTINGS mode");
    Logger_spi_cmd(STM32_CMD_SETTINGS_MODE);
    
    if (recvbuf[0] != STM32_RESP_OK)
    {
        ESP_LOGI(TAG, "Unable to put STM32 into SETTINGS mode");
        return RET_NOK;
    }

    Logger_spi_cmd(STM32_CMD_SET_RESOLUTION);
    if (recvbuf[0] != STM32_RESP_OK)
    {
        ESP_LOGI(TAG, "Unable to set STM32 ADC resolution");
        return RET_NOK;
    }

    Logger_spi_cmd(STM32_CMD_SET_SAMPLE_RATE);
    if (recvbuf[0] != STM32_RESP_OK)
    {
        ESP_LOGI(TAG, "Unable to set STM32 sample rate");
        return RET_NOK;
    }

    // Send settings one by one and confirm
    Logger_spi_cmd(STM32_CMD_MEASURE_MODE);
    if (recvbuf[0] != STM32_RESP_OK)
    {
        ESP_LOGI(TAG, "Unable to set STM32 in measure mode");
        return RET_NOK;
    }
    ESP_LOGI(TAG, "Sync done");
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
        gpio_set_level(GPIO_ADC_EN, 1);
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
        gpio_set_level(GPIO_ADC_EN, 0);
        _nextLoggerState = LOGGER_IDLE;
        return RET_OK;
    } 
    else 
    {
        return RET_NOK;
    }
}



void Logger_log()
{
   
    ulNotificationValue = ulTaskNotifyTake( 
                                            // xArrayIndex,
                                            pdTRUE,
                                            xMaxBlockTime );
    if( ulNotificationValue == 1 )
    {
        /* The transmission ended as expected. */
        spi_device_transmit(handle, &_spi_transaction);
        
        // Check if we are writing CSVs or raw data. 
        if (settings_get_logmode() == LOGMODE_CSV)
        {
             for (int j = 0; j < (SPI_BUFFERSIZE); j = j + 2)
            {
                // we'll have to multiply this with 20V/4096 = 0.00488281 V per LSB
                // Or in fixed point Q6.26 notation 488281 = 1 LSB
                
                // What we want to achieve here is 20V/4095 - 10V, but then in fixed point notation. 
                // To achieve that we will multiply the numbers by 1000000.
                // Instead of dividing the number by 4095 or use 0.0488281, we divide by the byte shift of 1<<12 which is more accurate with int32_t. 
                
                // Next steps can be merged, but are now seperated for checking values
                // First shift the bytes to get the ADC value
                t0 = ((int32_t)recvbuf[j] | ((int32_t)recvbuf[j + 1] << 8));
                t1 = t0 * (20LL * 1000000LL);
                t2 = t1 / ((1 << 12) - 1);
                t3 = t2 - 10000000LL;
                tbuffer_i32[writeptr] = (int32_t)t3;
                // ESP_LOGI(TAG, "%d, %d, %lld, %d", recvbuf[j], recvbuf[j+1], t3, tbuffer_i32[writeptr]);
                writeptr++;
                writeptr = writeptr % SD_BUFFERSIZE;
            }
        } else { // raw bytes writing
            memcpy(tbuffer+writeptr, recvbuf, SPI_BUFFERSIZE);
            writeptr = (writeptr + SPI_BUFFERSIZE) % SD_BUFFERSIZE;
        }

        // if previous write was halfway the SD buffersize, then start writing
        // Need to change this writes / second or so.
        if (writeptr == (SD_BUFFERSIZE / 2))
        {
            if (settings_get_logmode() == LOGMODE_CSV)
            {
                esp_sd_card_csv_write(tbuffer_i32, SD_BUFFERSIZE / 2);
            }  else {

            }   esp_sd_card_write(tbuffer, SD_BUFFERSIZE / 2);
            ESP_LOGI(TAG, "Half");
        }
        
        // If we reached the end of the SD buffer then write again.
        if (writeptr == 0)        
        {
            if (settings_get_logmode() == LOGMODE_CSV)
            {
                esp_sd_card_csv_write(tbuffer_i32+SD_BUFFERSIZE/2, SD_BUFFERSIZE / 2);
            } else {
                esp_sd_card_write(tbuffer+SD_BUFFERSIZE/2, SD_BUFFERSIZE / 2);
            }
            
            ESP_LOGI(TAG, "Full");
        }


    }
    else
    {
        /* The call to ulTaskNotifyTake() timed out. */
        // Is the STM32 hanging? 

    }

    
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
        .max_transfer_sz = 16,
        .flags = SPICOMMON_BUSFLAG_MASTER
    };

    //Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .clock_speed_hz=400000,
        .duty_cycle_pos=128,        //50% duty cycle
        .mode=0,
        .spics_io_num=GPIO_CS,
        .cs_ena_posttrans=3,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=4
    };  

    gpio_config_t adc_en_conf={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1<<GPIO_ADC_EN)
    };

    //GPIO config for the handshake line.
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_POSEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=1,
        .pin_bit_mask=(1<<GPIO_DATA_RDY_PIN)
    };

    // Init STM32 ADC enable pin
    gpio_set_level(GPIO_ADC_EN, 0);
    // Initialize SD card
    esp_sd_card_init();

     //Set up handshake line (DATA_RDY) interrupt.
    gpio_config(&io_conf);
    gpio_set_intr_type(GPIO_DATA_RDY_PIN, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_DATA_RDY_PIN, gpio_handshake_isr_handler, NULL);


    gpio_config(&adc_en_conf);
    gpio_set_level(GPIO_ADC_EN, 0);


    memset(&_spi_transaction, 0, sizeof(_spi_transaction));

    // //Create the semaphore.
    // // rdySem=xSemaphoreCreateBinary();

    // //Initialize the SPI bus and add the device we want to send stuff to.
    ret=spi_bus_initialize(SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(SENDER_HOST, &devcfg, &handle);
    assert(ret==ESP_OK);
    // // take the bus and never let go :-)
    ret = spi_device_acquire_bus(handle, portMAX_DELAY);
    assert(ret==ESP_OK);

    _spi_transaction.length=sizeof(sendbuf)*8; // size in bits
    _spi_transaction.rxlength = sizeof(recvbuf)*8; // size in bits
    _spi_transaction.tx_buffer=sendbuf;
    _spi_transaction.rx_buffer=recvbuf;
    _spi_transaction.tx_buffer=NULL;
    _spi_transaction.rx_buffer=recvbuf;
    writeptr = 0;
    ESP_LOGI(TAG, "Logger task started");
    while(1) {
       

        switch (_currentLoggerState)
        {
            case LOGGER_IDLE:
                vTaskDelay(500 / portTICK_PERIOD_MS);
                
            break;

            case LOGGER_LOGGING:
                
                Logger_log();
                if (_nextLoggerState !=LOGGER_LOGGING)
                {
                    // to be replaced by file close
                    esp_sd_card_close_unmount();
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
            ESP_LOGI(TAG, "Changing LOGGER state from %d to %d", _currentLoggerState, _nextLoggerState);
            _currentLoggerState = _nextLoggerState;
        }


    }
     
    
}

