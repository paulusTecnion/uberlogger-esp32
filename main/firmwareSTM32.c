#include <stdio.h>
#include  <string.h>
#include "config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_sd_card.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "FLASHING"

#define UART_TX_PIN GPIO_STM32_UART_RX
#define UART_RX_PIN GPIO_STM32_UART_TX
#define UART_BAUDRATE 115200
#define FLASH_START_ADDR 0x08000000
#define FLASH_PAGE_SIZE 1024

// Flash command definitions
#define CMD_GET_ID       0x02
#define CMD_WRITE_MEMORY 0x31
#define CMD_GO           0x21
#define CMD_ERASE_PAGE_1_2  0x43
#define CMD_ERASE_PAGE_2_2  0xBC
#define CMD_WRITE_UNPROTECT_1_2 0x73
#define CMD_WRITE_UNPROTECT_2_2 0x8C
#define CMD_ACTIVATE     0x7F
#define ACK              0x79
#define NACK             0x1F

static void send_byte(uint8_t byte)
{
    uart_write_bytes(UART_NUM_1, (const char*)&byte, 1);
}

static uint8_t recv_byte()
{
    uint8_t data;
    uart_read_bytes(UART_NUM_1, &data, 1, 1000 / portTICK_PERIOD_MS);
    return data;
}

static void recv_bytes(uint8_t* data, uint32_t len)
{
    uart_read_bytes(UART_NUM_1, data, len, 1000 / portTICK_PERIOD_MS);
}

static void send_data(uint8_t* data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        send_byte(data[i]);
    }
}

static void send_cmd(uint8_t cmd)
{
    send_byte(cmd);
    
}

static uint8_t recv_ack()
{
    uint8_t ack = recv_byte();
    if (ack == ACK)
    {
        return 1;
    }
    else if (ack == NACK)
    {
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "Invalid ACK/NACK response: %02X", ack);
        return 0;
    }
}

static esp_err_t flash_wipe()
{
    
    // uint8_t cmd[5] = {CMD_ERASE_PAGE_1_2, CMD_ERASE_PAGE_2_2, 0x00, 0x00, 0x00};
    uint8_t cmd[2];
    uint8_t rxBuf[15];
    memset(rxBuf, 0, sizeof(rxBuf));
    
    cmd[0]= 0x00;
    cmd[1]= 0xFF;

    send_data(cmd, 2);

    while(1)
    {
        recv_byte(rxBuf[0]);
        if (rxBuf != 0x00)
        {            
            ESP_LOGI(TAG, "0x%02X", rxBuf[0]);
            rxBuf[0] = 0x00;
        } else {
            ESP_LOGE(TAG, "Only got zeros");
        }
    }

    

    for (int i = 0; i < 2; i++)
    {
        cmd[0] = recv_byte();
        ESP_LOGI(TAG, "0x%02X", cmd[0]);
    }

    cmd[0] =  CMD_WRITE_UNPROTECT_1_2;
    cmd[1] =  CMD_WRITE_UNPROTECT_2_2; 
    
    send_data(cmd, 2);
    
    // Require 2 ACKs for this command
    for (int i = 0; i< 2; i++)
    {
        if (!recv_ack())
        {
            ESP_LOGE(TAG, "Got NACK for Write unprotect. Attempts %d", i);
            return ESP_FAIL;
        }
    }
    

    cmd[0] = CMD_ERASE_PAGE_1_2;
    cmd[1] = CMD_ERASE_PAGE_2_2;

    send_data(cmd, 2);
    if (!recv_ack())
    {
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Erased flash memory");

    return ESP_OK;  
}



static esp_err_t flash_write(uint32_t addr, uint8_t* data, uint32_t len)
{
    uint8_t cmd[5] = {CMD_WRITE_MEMORY, len - 1, 0, (addr >> 24) & 0xFF, (addr >> 16) & 0xFF};
    send_data(cmd, 5);
    if (!recv_ack())
    {
        ESP_LOGE(TAG, "Got NACK for Write Memory command");
        return ESP_FAIL;
    }
    
    send_cmd(CMD_GET_ID);
    if (!recv_ack())
    {
        ESP_LOGE(TAG, "Got NACK for Get ID command");
        return ESP_FAIL;
    }
     
    uint8_t data_addr[2] = {(addr >> 8) & 0xFF, addr & 0xFF};
    send_data(data_addr, 2);
    if (!recv_ack())
    {
        ESP_LOGE(TAG, "Got NACK for data address");
        return ESP_FAIL;
    }

    send_data(data, len);
    if (recv_ack())
    {
        ESP_LOGI(TAG, "Wrote %ld bytes to address 0x%08lX", len, addr);
    } else {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}


static void flash_jump_to(uint32_t addr)
{
    uint8_t cmd[5] = {CMD_GO, (addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF};
    send_data(cmd, 5);
    if (recv_ack())
    {
    ESP_LOGI(TAG, "Jumped to address 0x%08lX", addr);
    }
}

esp_err_t flash_stm32()
{
    ESP_LOGI(TAG, "Booting STM32G030 into bootloader mode...");
    gpio_set_level(GPIO_STM32_BOOT0, 1);
    gpio_set_level(GPIO_STM32_NRESET, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_STM32_NRESET, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    
    ESP_LOGI(TAG, "Starting STM32G030 flashing...");
    esp_err_t err = ESP_OK;
    FILE *file = NULL;

    // Write firmware data to OTA partition
    const int buff_size = 1024;
    char *buffer = malloc(buff_size);

    if (esp_sd_card_mount() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SD card");
        err = ESP_FAIL;
        goto error;
    }

    // Initialize UART driver
    uart_config_t uart_config = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 2048, 2048, 0, NULL, 0);

    // activating flash mode 
    
    send_cmd(CMD_ACTIVATE);
    if (recv_ack())
    {
        ESP_LOGI(TAG, "STM32G030 in flash mode");
    } else {
        err = ESP_FAIL;
        goto error;
    }

    // Erase flash pages
    ESP_LOGI(TAG, "Erasing pages");
    if (flash_wipe() == ESP_OK)
    {
        ESP_LOGI(TAG, "Pages erased");
    } else {
        ESP_LOGE(TAG, "Failed to erase pages");
        err = ESP_FAIL;
        goto error;
    }

        // Open firmware file on SD card
    ESP_LOGI(TAG, "Opening firmware")  ;
    file = fopen("/sdcard/stm32g030c6.bin", "rb");
    if (!file) {
        ESP_LOGE(TAG, "Error opening firmware file.\n");
        err = ESP_FAIL;
        goto error;
    }

    ESP_LOGI(TAG, "Flashing firmware...")  ;
    while (1) {
        int bytes_read = fread(buffer, 1, buff_size, file);
        if (bytes_read == 0) {
            break;
        }
        // esp_err_t err = esp_ota_write(ota_handle, buffer, bytes_read);
        err = flash_write(FLASH_START_ADDR, bytes_read, sizeof(bytes_read));
        if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing firmware to OTA partition: %d\n", err);
        
        err = ESP_FAIL;
        goto error;
        }
    }
    


    // Jump to application code
    flash_jump_to(FLASH_START_ADDR);

    ESP_LOGI(TAG, "Flashing complete.");
    esp_sd_card_unmount();

error:    
    free(buffer);
    fclose(file);
    uart_driver_delete(UART_NUM_1);

    if (err!=ESP_OK)
    {
        ESP_LOGE(TAG, "Firmware flashing failed!");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "Booting STM32G030 into normal mode...");
        gpio_set_level(GPIO_STM32_BOOT0, 0);
        gpio_set_level(GPIO_STM32_NRESET, 0);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_STM32_NRESET, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        return ESP_OK;
    }
    


}