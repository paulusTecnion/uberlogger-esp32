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
#include "esp_ota_ops.h"
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



TaskHandle_t xHandle_stm32 = NULL;
TaskHandle_t xHandle_oled = NULL;

static const char* TAG = "MAIN";
#define MDNS_INSTANCE "esp home web server"

// Prototypes for functions in other files
void task_hmi(void* ignore);
void task_logging(void * pvParameters);
void init_console();


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

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    // Create default loop for event handler for Wifi, REST etc.
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register console commands
    init_console();

    vTaskDelay (200/portTICK_PERIOD_MS);
    
    // esp_log_level_set("wifi", ESP_LOG_ERROR);
    // esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);

    #ifdef DEBUG_MAIN
    ESP_LOGI(TAG, "\r\n"
    "#################################\r\n"
    "#           UberLogger          #\r\n"
    "#################################");
    #endif

    ESP_ERROR_CHECK(init_fs());

    // Leave these lines BEFORE settings_init(), else the SSID will not be set correctly the first time!!
    if (settings_get_boot_reason() ==0)
    {
        wifi_init();
    }
    settings_init();

    // The wifi seems to be either crashing the ESP sometimes due to this: https://github.com/espressif/esp-idf/issues/7404
    // Or it uses too much current which resets the ESP internally. Either way, the next delay seems to fix this issue for now...
    vTaskDelay (1000/portTICK_PERIOD_MS);
    
    if (settings_get_boot_reason() ==0)
    {
        wifi_start();
    }

    // Tasks to spin up logging and HMI
    xTaskCreate(task_logging, "task_logging", 4500, NULL, 25, &xHandle_stm32);
    xTaskCreate(task_hmi, "task_hmi", 2000, NULL, tskIDLE_PRIORITY, &xHandle_oled);
    
    if (settings_get_boot_reason() == 0)
    {
        ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));
    }
  
}