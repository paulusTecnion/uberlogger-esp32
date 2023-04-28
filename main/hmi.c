#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#include "config.h"
#include "logger.h"


#define TAG "HMI"

void task_hmi(void* ignore) {
//   u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
//   u8g2_esp32_hal.sda = GPIO_OLED_SDA;
//   u8g2_esp32_hal.scl = GPIO_OLED_SCL;
//   u8g2_esp32_hal_init(u8g2_esp32_hal);

//   u8g2_t u8g2;  // a structure which will contain all the data for one display
//   u8g2_Setup_ssd1306_i2c_128x32_univision_f(
//       &u8g2, U8G2_R0,
//       // u8x8_byte_sw_i2c,
//       u8g2_esp32_i2c_byte_cb,
//       u8g2_esp32_gpio_and_delay_cb);  // init u8g2 structure
//   u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);

// //   ESP_LOGI(TAG, "u8g2_InitDisplay");
//   u8g2_InitDisplay(&u8g2);  // send init sequence to the display, display is in
//                             // sleep mode after this,

// //   ESP_LOGI(TAG, "u8g2_SetPowerSave");
//   u8g2_SetPowerSave(&u8g2, 0);  // wake up display
// //   ESP_LOGI(TAG, "u8g2_ClearBuffer");
//   u8g2_ClearBuffer(&u8g2);
// //   ESP_LOGI(TAG, "u8g2_DrawBox");
  
//   u8g2_DrawFrame(&u8g2, 0, 26, 100, 6);

// //   ESP_LOGI(TAG, "u8g2_SetFont");
//   u8g2_SetFont(&u8g2, u8g2_font_smart_patrol_nbp_tf );
// //   ESP_LOGI(TAG, "u8g2_DrawStr");
//   u8g2_DrawStr(&u8g2, 2, 17, "UberLOGGER");
// //   ESP_LOGI(TAG, "u8g2_SendBuffer");
//   u8g2_SendBuffer(&u8g2);
//   int i=0;

  // Required for red LED. Disable JTAG functionality.
  gpio_reset_pin(GPIO_HMI_LED_RED);

  gpio_set_direction(GPIO_HMI_LED_GREEN, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_HMI_LED_RED, GPIO_MODE_OUTPUT);

  static uint8_t timerCounter = 0;
  static uint8_t toggle_green = 1;
  static uint8_t toggle_red = 0;
  static uint8_t startLogging = 1;

  while(1)
  {
    // if (i > 100)
    // {
    //     u8g2_SetDrawColor(&u8g2,0);
    // } else {
    //     u8g2_SetDrawColor(&u8g2,1);
    // }
    // u8g2_DrawBox(&u8g2, 0, 26, i, 6);
    // // u8g2_SendBuffer(&u8g2);
    
    // u8g2_SendBuffer(&u8g2);
    // if (i > 100)
    // {
    //     i = 0;
    // }
    // i = i + 20;
    



    if (Logger_getError() > 0)
    {
        if (timerCounter % 2 == 0)
        {
            toggle_red = !toggle_red;
        }
          // ESP_LOGI(TAG, "Toggle red");
    } else {
      toggle_red = 0;
    }

    switch (Logger_getState())
    {
        case LOGTASK_IDLE:
          toggle_green = 1;
          // This variable is used to indicate that logging just started when the button is pressed.
          startLogging = 1;
        break;

        case LOGTASK_LOGGING:
          if (startLogging)
          {
            // If we just started logging, we want to immediately turn off the LED to give immediate feedback to the user.
            startLogging = 0;
            toggle_green = 0;
          }

          if (timerCounter % 10 == 0)
          {
            toggle_green = !toggle_green;
          }
          
          
        break;
    }

    
    
    gpio_set_level(GPIO_HMI_LED_GREEN, toggle_green);
    gpio_set_level(GPIO_HMI_LED_RED, toggle_red);
    timerCounter++;
    vTaskDelay(100 / portTICK_PERIOD_MS);

  }
  // ESP_LOGI(TAG, "All done!");

  // vTaskDelete(NULL);
}