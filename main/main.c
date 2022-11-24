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