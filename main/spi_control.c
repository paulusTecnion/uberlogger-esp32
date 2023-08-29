#include "spi_control.h"

#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "logger.h"
#include "settings.h"
#include "config.h"

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

static const char* TAG_SPI_CTRL = "SPI_CTRL";

// Buffer for sending data to the STM
DMA_ATTR uint8_t sendbuf[STM_SPI_BUFFERSIZE_CMD_TX];
// Buffer for receiving data from the STM
DMA_ATTR uint8_t recvbuf0[sizeof(spi_msg_1_t)];

// handle to spi device
spi_device_handle_t stm_spi_handle;
// transactions variables for doing spi transactions with stm
spi_transaction_t _spi_transaction_rx0, _spi_transaction_rx1;


// Interrupt notification value
uint32_t ulNotificationValue = 0;

// max block time for interrupt
const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 100 );

// Handle to stm32 task
extern TaskHandle_t xHandle_stm32;

volatile rxdata_state_t rxdata_state = RXDATA_STATE_NODATA;

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
    // int_level = gpio_get_level(GPIO_DATA_RDY_PIN);

    // if (int_level)
    // int_level = 1;

    
    vTaskNotifyGiveFromISR( xHandle_stm32,
                                //    xArrayIndex,
                                   &xYieldRequired );
    
    portYIELD_FROM_ISR(xYieldRequired);
}

esp_err_t spi_ctrl_datardy_int(uint8_t value) 
{
    if (value == 1)
    {
        #ifdef DEBUG_SPI_CONTROL
        ESP_LOGI(TAG_SPI_CTRL, "Enabling data_rdy interrupts");
        #endif
        // Trigger on up and down edges
        if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK)
        {
            ESP_LOGE(TAG_SPI_CTRL, "Unable to install ISR service");
            return ESP_FAIL;
        }
        // gpio_set_intr_type(GPIO_DATA_RDY_PIN, GPIO_INTR_POSEDGE) == ESP_OK &&
        if (gpio_isr_handler_add(GPIO_DATA_RDY_PIN, gpio_handshake_isr_handler, NULL) != ESP_OK)
        {
            ESP_LOGE(TAG_SPI_CTRL, "Unable to add ISR handler");
            return ESP_FAIL;
        }
    
        return ESP_OK;
    } else if (value == 0) {
        #ifdef DEBUG_SPI_CONTROL
        ESP_LOGI(TAG_SPI_CTRL, "Removing ISR handler" );
        #endif
        gpio_isr_handler_remove(GPIO_DATA_RDY_PIN);
        #ifdef DEBUG_SPI_CONTROL
        ESP_LOGI(TAG_SPI_CTRL, "Uninstalling ISR service");
        #endif
        gpio_uninstall_isr_service();
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }

}

esp_err_t spi_ctrl_init(uint8_t spicontroller, uint8_t gpio_data_ready_point)
{
    esp_err_t ret;
     //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=STM32_SPI_MOSI,
        .miso_io_num=STM32_SPI_MISO,
        .sclk_io_num=STM32_SPI_SCLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz = 8192,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };

    //Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .clock_speed_hz=SPI_STM32_BUS_FREQUENCY, //400000,
        .duty_cycle_pos=128,        //50% duty cycle
        .mode=0,                    // SPI mode 0,
        .spics_io_num= STM32_SPI_CS, //STM32_SPI_CS,//GPIO_CS,
        .cs_ena_posttrans=1,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=3,
        .flags = 0,
        .input_delay_ns= 11    // (50 ns GPIO matrix ESP32) + 11 ns STM32
    };  

    
    //GPIO config for the handshake line. Only trigger on positive edge
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_POSEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=0,
        .pin_bit_mask=(1<<gpio_data_ready_point)
    };

    gpio_config(&io_conf);

    gpio_set_drive_capability(STM32_SPI_MOSI, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(STM32_SPI_SCLK, GPIO_DRIVE_CAP_3);

    memset(&_spi_transaction_rx0, 0, sizeof(_spi_transaction_rx0));
    memset(&_spi_transaction_rx1, 0, sizeof(_spi_transaction_rx1));
    memset(recvbuf0, 0 , sizeof(recvbuf0));

    
    // //Initialize the SPI bus and add the device we want to send stuff to.
    ret=spi_bus_initialize(spicontroller, &buscfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(spicontroller, &devcfg, &stm_spi_handle);
    assert(ret==ESP_OK);
    // // take the bus and never let go :-)
    ret = spi_device_acquire_bus(stm_spi_handle, portMAX_DELAY);
    assert(ret==ESP_OK);
    
    return ESP_OK;
    
}



esp_err_t spi_ctrl_single_transaction(spi_transaction_t * transaction)
{
    
    if (spi_device_queue_trans(stm_spi_handle, transaction, 50/ portTICK_PERIOD_MS) == ESP_OK)
    {
        if (spi_device_get_trans_result(stm_spi_handle, &transaction, 2000 / portTICK_PERIOD_MS) == ESP_OK)
        {
            return ESP_OK;
        }   
    }

    return ESP_FAIL;
}


esp_err_t spi_ctrl_cmd(stm32cmd_t cmd, spi_cmd_t* cmd_data, size_t rx_data_length)
{

    // spi_cmd_t spi_cmd;
    uint8_t timeout = 0;
    
    // spi_cmd.command = cmd;
    // spi_cmd.data = cmd_data;    
    
    _spi_transaction_rx0.length = sizeof(spi_cmd_t)*8; // in bits!
    _spi_transaction_rx0.rxlength = sizeof(spi_cmd_t)*8;
    _spi_transaction_rx0.rx_buffer = NULL;
    _spi_transaction_rx0.tx_buffer = (const void*)cmd_data;

    // Temporarily disable interrupts
   
    // ESP_LOGE(TAG_LOG, "Waiting for data rdy pin low..");

    // Make sure data ready pin is low

    // for (int i=0; i<7; i++)
    // {
    //     ESP_LOGI(TAG_SPI_CTRL, "data%d %d", i, *((&cmd_data->data0) + i));
    // }

    while(gpio_get_level(GPIO_DATA_RDY_PIN))
    {
        vTaskDelay( 10 / portTICK_PERIOD_MS);
        timeout++;
        if (timeout >= 200)
        {
            ESP_LOGE(TAG_SPI_CTRL, "Command %d failed. STM32 has data rdy still high, expected low 1/3", cmd);
            
            return ESP_FAIL;
        }
    }

    
    
    if (spi_ctrl_single_transaction(&_spi_transaction_rx0) != ESP_OK)
    {   
        ESP_LOGE(TAG_SPI_CTRL, "SPI command transmission timeout");
        return ESP_FAIL;
    }
    // ESP_LOGI(TAG_LOG,"Pass 1/2 CMD");
    // assert(spi_device_polling_transmit(stm_spi_handle, &_spi_transaction) == ESP_OK);
    // wait for 5 ms for stm32 to process data
    
    timeout = 0;
    // Wait until data is ready for transmission (data rdy turns high)
    while(!gpio_get_level(GPIO_DATA_RDY_PIN))
    {
        vTaskDelay( 10 / portTICK_PERIOD_MS);
        timeout++;
        if (timeout >= 200)
        {
            ESP_LOGE(TAG_SPI_CTRL, "Command %d failed. STM32 was LOW, expected HIGH 2/3", cmd);
            return ESP_FAIL;
        }
    }

    _spi_transaction_rx0.rxlength = 8*rx_data_length;
    _spi_transaction_rx0.length = 8*rx_data_length;
    _spi_transaction_rx0.rx_buffer = recvbuf0;
    _spi_transaction_rx0.tx_buffer = NULL;

    
   if (spi_ctrl_single_transaction(&_spi_transaction_rx0) != ESP_OK)
    {   
        ESP_LOGE(TAG_SPI_CTRL, "STM32 reception failure");
        return ESP_FAIL;
    }
    
    timeout = 0;
    // Wait until data rdy pin is low again.
    while(gpio_get_level(GPIO_DATA_RDY_PIN))
    {
        vTaskDelay( 10 / portTICK_PERIOD_MS);
        timeout++;
        if (timeout >= 200)
        {
            ESP_LOGE(TAG_SPI_CTRL, "Command %d failed. STM32 was HIGH, expected LOW 3/3", cmd);
            return ESP_FAIL;
        }
    }

    
    return ESP_OK;

}

void spi_ctrl_print_rx_buffer()
{
    ESP_LOGI(TAG_SPI_CTRL, "recvbuf:");
    for (int i=0; i<16;i++)
    {
        ESP_LOGI(TAG_SPI_CTRL, "%d", recvbuf0[i]);
    }
}

rxdata_state_t spi_ctrl_rxstate()
{
    return rxdata_state;
}

esp_err_t spi_ctrl_queue_msg(uint8_t * txData, size_t length)
{
    _spi_transaction_rx0.length = length*8;
    _spi_transaction_rx0.rxlength= length*8;
    _spi_transaction_rx0.tx_buffer = txData;                 
    _spi_transaction_rx0.rx_buffer=(uint8_t*)&recvbuf0;

    // if(gpio_get_level(GPIO_DATA_RDY_PIN))
    // {
    //     // Apparently the STM32 is still in a busy state
    //     ESP_LOGE(TAG_SPI_CTRL, "STM32 still busy (DATA RDY HIGH)");
    //     return ESP_FAIL;
    // }

    // Queue transaction
    #ifdef DEBUG_SPI_CONTROL
    ESP_LOGI(TAG_SPI_CTRL, "Queuing message %d, %d", _spi_transaction_rx0.length, _spi_transaction_rx0.rxlength);
    #endif
    if(spi_device_queue_trans(stm_spi_handle, &_spi_transaction_rx0, 10 / portTICK_PERIOD_MS) != ESP_OK)
    {
        ESP_LOGE(TAG_SPI_CTRL, "Cannot queue msg");
        return ESP_FAIL;
    }

    return ESP_OK;


}


esp_err_t spi_ctrl_receive_data()
{
    
    
    spi_transaction_t * ptr = &_spi_transaction_rx0;
    // Retreive
    esp_err_t ret = spi_device_get_trans_result(stm_spi_handle, &ptr, 2000 / portTICK_PERIOD_MS);

    // if(ret == ESP_OK)
    // {
       
        // Logger_GetSingleConversion(&measurement);
        // ESP_LOGI(TAG_LOG, "%f, %f, %f, %d", measurement.analogData[0], measurement.analogData[1], measurement.temperatureData[0], measurement.timestamp);
        // ESP_LOGI(TAG_LOG,"Single shot done");
       
    // } 
    
    if (ret == ESP_ERR_TIMEOUT) 
    {
        ESP_LOGE(TAG_SPI_CTRL, "STM32 timeout; could not receive data");
        return ESP_ERR_TIMEOUT;
    }

    if (ret == ESP_ERR_INVALID_ARG )
    {
        ESP_LOGE(TAG_SPI_CTRL, "spi_device_get_trans_result arg error");
        return ESP_ERR_INVALID_ARG;
    }
     return ESP_OK;
   

}

uint8_t * spi_ctrl_getRxData()
{
    rxdata_state = RXDATA_STATE_NODATA;
    return recvbuf0;
}

void spi_ctrl_reset_rx_state()
{
    rxdata_state = RXDATA_STATE_NODATA;
}

void spi_ctrl_loop()
{
      ulNotificationValue = ulTaskNotifyTake( 
                                            // xArrayIndex,
                                            pdTRUE,
                                            xMaxBlockTime );
    

        // if (gpio_get_level(GPIO_DATA_OVERRUN))
        // {
            // rxdata_state = RXDATA_STATE_DATA_OVERRUN;
        // } else
         if (ulNotificationValue) {
            #ifdef DEBUG_SPI_CONTROL
            ESP_LOGI(TAG_SPI_CTRL, "HIGH TRIGGER");
            #endif
            rxdata_state = RXDATA_STATE_DATA_READY;
        }  

       
}