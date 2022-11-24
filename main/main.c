/* SPI Slave example, sender (uses SPI master driver)
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "u8g2.h"
#include "esp_log.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "argtable3/argtable3.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_sd_card.h"
#include "esp_timer.h"
#include "u8g2_esp32_hal.h"
#include "hw_config.h"


#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_EXAMPLE_TASK_STACK_SIZE)



TaskHandle_t xHandle_stm32 = NULL;
TaskHandle_t xHandle_oled = NULL;


const UBaseType_t xArrayIndex = 1;


esp_err_t start_rest_server(const char *base_path);

void task_hmi(void* ignore);
void task_logging(void * pvParameters);
void init_console();

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


// void task_direct_logging(void * pvParameters)
// {
//     esp_err_t ret;
//     spi_device_handle_t handle;

//     uint32_t ulNotificationValue;
//     uint32_t writeptr = NULL;
//     //Configuration for the SPI bus
//     spi_bus_config_t buscfg={
//         .mosi_io_num=GPIO_MOSI,
//         .miso_io_num=GPIO_MISO,
//         .sclk_io_num=GPIO_SCLK,
//         .quadwp_io_num=-1,
//         .quadhd_io_num=-1,
//         .max_transfer_sz = 16,
//         .flags = SPICOMMON_BUSFLAG_MASTER
//     };

//     //Configuration for the SPI device on the other side of the bus
//     spi_device_interface_config_t devcfg={
//         .command_bits=0,
//         .address_bits=0,
//         .dummy_bits=0,
//         // .clock_speed_hz=400000,
//         .clock_speed_hz=8000000,
//         .duty_cycle_pos=128,        //50% duty cycle
//         .mode=0,
//         .spics_io_num=GPIO_CS,
//         .cs_ena_posttrans=3,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
//         .queue_size=4
//     };

  

//     gpio_config_t adc_en_conf={
//         .mode=GPIO_MODE_OUTPUT,
//         .pin_bit_mask=(1<<GPIO_ADC_EN)
//     };

//     const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );

//     gpio_config(&adc_en_conf);
//     gpio_set_level(GPIO_ADC_EN, 0);

//     spi_transaction_t t;
//     memset(&t, 0, sizeof(t));

//     // //Create the semaphore.
//     // // rdySem=xSemaphoreCreateBinary();

//     // //Initialize the SPI bus and add the device we want to send stuff to.
//     ret=spi_bus_initialize(SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO);
//     assert(ret==ESP_OK);
//     ret=spi_bus_add_device(SENDER_HOST, &devcfg, &handle);
//     assert(ret==ESP_OK);
//     // // take the bus and never let go :-)
//     ret = spi_device_acquire_bus(handle, portMAX_DELAY);
//     assert(ret==ESP_OK);
//     // //Assume the slave is ready for the first transmission: if the slave started up before us, we will not detect
//     // //positive edge on the handshake line.
//     // xSemaphoreGive(rdySem);
    

    

//     t.length=sizeof(sendbuf)*8; // size in bits
//     t.rxlength = sizeof(recvbuf)*8; // size in bits
//     t.tx_buffer=sendbuf;
//     t.rx_buffer=recvbuf;
//     t.tx_buffer=NULL;
//     t.rx_buffer=recvbuf;
//     writeptr = 0;
    
//     while(1) {
//         // int res = snprintf(sendbuf, sizeof(sendbuf),
//         //         "Sender, transmission no. %04i.", n);
        
        
//         //Wait for slave to be ready for next byte before sending
//         // xSemaphoreTake(rdySem, portMAX_DELAY); //Wait until slave is ready
//         //


//         ulNotificationValue = ulTaskNotifyTake( 
//                                                 // xArrayIndex,
//                                                    pdTRUE,
//                                                    xMaxBlockTime );
//         if( ulNotificationValue == 1 )
//         {
//             /* The transmission ended as expected. */
//             ret=spi_device_transmit(handle, &t);
//             // ESP_LOGI("MAIN","%s, received length: %d", recvbuf, t.rxlength);
//             // printf("Received: %s\n", recvbuf);
//             memcpy(tbuffer+writeptr, recvbuf, BUFFERSIZE);
//             writeptr = (writeptr + BUFFERSIZE) % SDBUFFERSIZE;
            
//             // if previous write was 
//             if (writeptr == (SDBUFFERSIZE / 2))
//             {
//                 // ESP_LOGI(TAG, "Half");
//                 // trigger buffer half way
//                 // xTaskNotifyGive(task_sdcard_write_halfway);
//                 esp_sd_card_write(tbuffer, SDBUFFERSIZE / 2);
//             }
            

//             if (writeptr == NULL)        
//             {
//                 esp_sd_card_write(tbuffer+SDBUFFERSIZE/2, SDBUFFERSIZE / 2);
//                 // ESP_LOGI(TAG, "Full");
//                 //xTaskNotifyGive(task_sdcard_write_full);
//                 // trigger buffer full
//             }


//         }
//         else
//         {
//             /* The call to ulTaskNotifyTake() timed out. */

//         }
        
//     }
     

//     // //Never reached.
//     // ret=spi_bus_remove_device(handle);
//     // // assert(ret==ESP_OK);
//     // while(1)
//     // {
//     //     //vTaskDelay(100 / portTICK_PERIOD_MS);
//     //     vTaskSuspend(NULL);
//     // }
    
// }




//Main application
void app_main(void)
{

    // Register console commands
    init_console();
    
    //GPIO config for the handshake line.
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_POSEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=1,
        .pin_bit_mask=(1<<GPIO_DATA_RDY_PIN)
    };

    gpio_set_level(GPIO_ADC_EN, 0);
    esp_sd_card_init();

     //Set up handshake line interrupt.
    gpio_config(&io_conf);
    gpio_set_intr_type(GPIO_DATA_RDY_PIN, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_DATA_RDY_PIN, gpio_handshake_isr_handler, NULL);

    
    // Start tasks
    xTaskCreatePinnedToCore(task_logging, "task_logging", 3500, NULL, 3, &xHandle_stm32, 1);
    xTaskCreate(task_hmi, "oled_task", 4000, NULL, tskIDLE_PRIORITY, &xHandle_oled);
    ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));
    

    
}