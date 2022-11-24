#include <stdio.h>

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

#define BUFFERSIZE 16
#define SDBUFFERSIZE 8192


DMA_ATTR uint8_t sendbuf[BUFFERSIZE];
DMA_ATTR uint8_t recvbuf[BUFFERSIZE];
uint8_t tbuffer[SDBUFFERSIZE];
int32_t tbuffer_i32[SDBUFFERSIZE];
char strbuffer[16];

void task_logging(void * pvParameters)
{
    esp_err_t ret;
    spi_device_handle_t handle;

    uint32_t ulNotificationValue;
    uint32_t writeptr = 0;
    int64_t t0, t1,t2,t3;
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

    const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );

    gpio_config(&adc_en_conf);
    gpio_set_level(GPIO_ADC_EN, 0);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

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

    t.length=sizeof(sendbuf)*8; // size in bits
    t.rxlength = sizeof(recvbuf)*8; // size in bits
    t.tx_buffer=sendbuf;
    t.rx_buffer=recvbuf;
    t.tx_buffer=NULL;
    t.rx_buffer=recvbuf;
    writeptr = 0;
    
    while(1) {
       
        ulNotificationValue = ulTaskNotifyTake( 
                                                // xArrayIndex,
                                                   pdTRUE,
                                                   xMaxBlockTime );
        if( ulNotificationValue == 1 )
        {
            /* The transmission ended as expected. */
            ret=spi_device_transmit(handle, &t);
            
           

            for (int j = 0; j < (BUFFERSIZE); j = j + 2)
            {
                // we'll have to multiply this with 20V/4096 = 0.00488281 V per LSB
                // Or in fixed point Q6.26 notation 488281 = 1 LSB
                //tbuffer_i32[j] =  (int32_t)((int32_t)recvbuf[j-writeptr] | ((int32_t)recvbuf[(j-writeptr)+1] << 8)) * 4882;
                //  tbuffer_i32[j] = ((int32_t)recvbuf[x-writeptr] | ((int32_t)recvbuf[(x-writeptr)+1] << 8)) * (10LL * 1000000LL) / (1 << 12) - 10000000;
                t0 = ((int32_t)recvbuf[j] | ((int32_t)recvbuf[j + 1] << 8));
                t1 = t0 * (20LL * 1000000LL);
                t2 = t1 / ((1 << 12) - 1);
                t3 = t2 - 10000000LL;
                tbuffer_i32[writeptr] = (int32_t)t3;
                // ESP_LOGI(TAG, "%d, %d, %lld, %d", recvbuf[j], recvbuf[j+1], t3, tbuffer_i32[writeptr]);
                writeptr++;
                
            }

            writeptr = writeptr % SDBUFFERSIZE;
            

            // if previous write was 
            if (writeptr == (SDBUFFERSIZE / 2))
            {
                esp_sd_card_csv_write(tbuffer_i32, SDBUFFERSIZE / 2);
                ESP_LOGI(TAG, "Half");
            }
            
            if (writeptr == 0)        
            {
                esp_sd_card_csv_write(tbuffer_i32+SDBUFFERSIZE/2, SDBUFFERSIZE / 2);
                ESP_LOGI(TAG, "Full");
            }


        }
        else
        {
            /* The call to ulTaskNotifyTake() timed out. */

        }
        
    }
     
    
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
