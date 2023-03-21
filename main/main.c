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
#include "config.h"
#include "settings.h"
#include "sysinfo.h"

#include "esp_vfs_semihost.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "rest_server.h"

#include "wifi.h"

esp_err_t start_rest_server(const char *base_path);

TaskHandle_t xHandle_stm32 = NULL;
TaskHandle_t xHandle_oled = NULL;

static const char* TAG = "MAIN";
#define MDNS_INSTANCE "esp home web server"

// Prototypes for functions in other files
void task_hmi(void* ignore);
void task_logging(void * pvParameters);
void init_console();


// static void initialise_mdns(void)
// {
//     mdns_init();
//     mdns_hostname_set(CONFIG_EXAMPLE_MDNS_HOST_NAME);
//     mdns_instance_name_set(MDNS_INSTANCE);

//     mdns_txt_item_t serviceTxtData[] = {
//         {"board", "esp32"},
//         {"path", "/"}
//     };

//     ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
//                                      sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
// }




#if CONFIG_EXAMPLE_WEB_DEPLOY_SF
esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = "www",
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("www", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}
#endif


//Main application
void app_main(void)
{


    init_console();
    // Register console commands
    vTaskDelay (200/portTICK_PERIOD_MS);
    // esp_log_level_set("wifi", ESP_LOG_ERROR);
    // esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);

    #ifdef DEBUG_MAIN
    ESP_LOGI(TAG, "\r\n"
    "#################################\r\n"
    "#           UberLogger          #\r\n"
    "#################################");
    #endif


 
    settings_init();
    sysinfo_init();

  // Start tasks
    xTaskCreate(task_logging, "task_logging", 3500, NULL, 8, &xHandle_stm32);
    xTaskCreate(task_hmi, "task_hmi", 2000, NULL, tskIDLE_PRIORITY, &xHandle_oled);
    
    
    // wifi_init_sta();
    // wifi_init_softap();
    // esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    // esp_wifi_set_max_tx_power(60); // corresponding to 15 dBi
    wifi_start();
    // The wifi seems to be either crashing the ESP sometimes due to this: https://github.com/espressif/esp-idf/issues/7404
    // Or it uses too much current which resets the ESP internally. Either way, the next delay seems to fix this issue for now...
    vTaskDelay (1000/portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(init_fs());
    ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));

  

    

    
}