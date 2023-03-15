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

#define TAG "FW-STM32"
#define FILE_STM32 "/sdcard/ota_support.bin"

#define UART_TX_PIN GPIO_STM32_UART_RX
#define UART_RX_PIN GPIO_STM32_UART_TX
#define UART_BAUDRATE 115200 
#define UART_PORT UART_NUM_0
#define FLASH_START_ADDR 0x08000000UL
#define FLASH_PAGE_SIZE 1024

// Flash command definitions
#define CMD_GET_ID       0x02
#define CMD_WRITE_MEMORY_1_2 0x31
#define CMD_WRITE_MEMORY_2_2 0xCE
#define CMD_GO_1_2           0x21
#define CMD_GO_2_2           0xDE
#define CMD_EXTENDED_ERASE_1_2  0x44
#define CMD_EXTENDED_ERASE_2_2  0xBB
#define CMD_WRITE_UNPROTECT_1_2 0x73
#define CMD_WRITE_UNPROTECT_2_2 0x8C
#define CMD_ACTIVATE     0x7F
#define ACK              0x79
#define NACK             0x1F



static void send_byte(uint8_t byte)
{
    // uart_write_bytes(UART_PORT, (const char*)&byte, 1);
    uart_write_bytes(UART_PORT, (const char*)&byte, 1);
    ESP_ERROR_CHECK(uart_wait_tx_done(UART_PORT, 100)); 
}

static uint8_t recv_byte()
{
    uint8_t data;
    uart_read_bytes(UART_PORT, &data, 1, 2000 / portTICK_PERIOD_MS);
    return data;
}

static void recv_bytes(uint8_t* data, uint32_t len)
{
    uart_read_bytes(UART_PORT, data, len, 1000 / portTICK_PERIOD_MS);
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

static uint8_t recv_resp(uint8_t resp)
{
    uint8_t ack = recv_byte();
    if (ack == resp)
    {
        return 1;
    } else {
        ESP_LOGE(TAG, "Unexpected response: %02X", ack);
        return 0;
    }
}

static esp_err_t flash_wipe()
{
    
    // uint8_t cmd[5] = {CMD_ERASE_PAGE_1_2, CMD_ERASE_PAGE_2_2, 0x00, 0x00, 0x00};
    uint8_t cmd[2];
    uint8_t rxBuf[15];
    memset(rxBuf, 0, sizeof(rxBuf));
        

    cmd[0] = CMD_EXTENDED_ERASE_1_2;
    cmd[1] = CMD_EXTENDED_ERASE_2_2;

    send_data(cmd, 2);
    if (!recv_ack())
    {
        return ESP_FAIL;
    }

    // Send mass erase code
    cmd[0] = 0xFF;
    cmd[1] = 0xFF;

    send_data(cmd, 2);

    // Send checksum
    cmd[0] = 0x00;
    send_data(cmd, 1);  

    // Wait for ack
    if (!recv_ack())
    {
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Erased flash memory");

    return ESP_OK;  
}



static esp_err_t flash_write(uint32_t addr, uint8_t *data, uint32_t len)
{
    // uint8_t cmd[5] = {CMD_WRITE_MEMORY, len - 1, 0, (addr >> 24) & 0xFF, (addr >> 16) & 0xFF};
    uint8_t cmd[5];
    cmd[0] = CMD_WRITE_MEMORY_1_2;
    cmd[1] = CMD_WRITE_MEMORY_2_2;
    unsigned int aligned_len;
    
    uint8_t tmp[256 + 2];

    if (addr & 0x3)
    {
        ESP_LOGE(TAG, "Address must be 4-byte aligned");
        return ESP_FAIL;
    }

    // Align to 256 bytes
    aligned_len = (len + 3) & ~3;

    // ESP_LOGI(TAG, "Sending Write Memory command");

    send_data(cmd, 2);
    if (!recv_ack())
    {
        ESP_LOGE(TAG, "Got NACK for Write Memory command");
        return ESP_FAIL;
    }

    // Send start address aand checksum
    cmd[0] = (addr >> 24) & 0xFF;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = (addr) & 0xFF;
    // Checksum
    cmd[4] = cmd[0] ^ cmd[1] ^ cmd[2] ^ cmd[3];
     
    // ESP_LOGI(TAG, "Sending start address and checksum");
    send_data(cmd, 5);
    if (!recv_ack())
    {
        ESP_LOGE(TAG, "Got NACK for data address");
        return ESP_FAIL;
    }

    // ESP_LOGI(TAG, "Sending length, data and checksum bytes");
    // Send length and checksum
    uint8_t checksum = aligned_len - 1;
     // Send the number of bytes to be received
    tmp[0] = aligned_len - 1;
    // Fill data bytes, calculate checksum
    for (int i = 0; i < aligned_len; i++) {
        checksum ^= data[i];
        tmp[i+1] = data[i];
    }

    // padding data bytes
	for (int i = len; i < aligned_len; i++) {
		checksum ^= 0xFF;
		tmp[i + 1] = 0xFF;
	}

    tmp[len+1]  = checksum;
    send_data(tmp, len+2);

    
    if (recv_ack())
    {
        ESP_LOGI(TAG, "Wrote %ld bytes to address 0x%08lX", len, addr);
    } else {
        ESP_LOGE(TAG, "Got NACK for data");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}


static void flash_jump_to(uint32_t addr)
{
    uint8_t cmd[5];

    cmd[0] = CMD_GO_1_2;
    cmd[1] = CMD_GO_2_2;

    send_data(cmd, 2);

    if (!recv_ack())
    {
        ESP_LOGE(TAG, "Got NACK for GO command");
        return;
    }

    // Send start address aand checksum
    cmd[0] = (addr >> 24) & 0xFF;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = (addr) & 0xFF;
    // Checksum
    cmd[4] = cmd[0] ^ cmd[1] ^ cmd[2] ^ cmd[3];


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
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_STM32_NRESET, 1);
    // vTaskDelay(500 / portTICK_PERIOD_MS);


   
    esp_err_t err = ESP_OK;
    FILE *file = NULL;

    // Write firmware data to OTA partition
    const int buff_size = 256;
    uint8_t *buffer = (uint8_t*)malloc(buff_size);

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
        .source_clk = UART_SCLK_DEFAULT,
    };

     

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
 

    // activating flash mode 
     ESP_LOGI(TAG, "Starting STM32G030 bootloader");
    send_cmd(CMD_ACTIVATE);
    if (recv_ack())
    {
        ESP_LOGI(TAG, "STM32G030 in bootloader mode");
    } else {
        err = ESP_FAIL;
        goto error;
    }

    // Erase flash pages
    ESP_LOGI(TAG, "Mass erasing");
    if (flash_wipe() == ESP_OK)
    {
        ESP_LOGI(TAG, "Flash erased");
    } else {
        ESP_LOGE(TAG, "Failed to erase pages");
        err = ESP_FAIL;
        goto error;
    }

        // Open firmware file on SD card
    ESP_LOGI(TAG, "Opening firmware")  ;
    file = fopen(FILE_STM32, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Error opening firmware file.\n");
        err = ESP_FAIL;
        goto error;
    }

    ESP_LOGI(TAG, "Flashing firmware...")  ;
    uint32_t write_address = FLASH_START_ADDR;


    while (1) {
        uint32_t bytes_read = fread(buffer, 1, buff_size, file);
        if (bytes_read == 0) {
            break;
        }
        // esp_err_t err = esp_ota_write(ota_handle, buffer, bytes_read);
        err = flash_write(write_address, buffer, bytes_read);
        write_address += bytes_read;

        if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing firmware: %d\n", err);
        
        err = ESP_FAIL;
        goto error;
        } 
    }
    


    // Jump to application code
    flash_jump_to(FLASH_START_ADDR);

    ESP_LOGI(TAG, "Flashing complete.");
    fclose(file);
    esp_sd_card_unmount();

error:    
    free(buffer);

    uart_driver_delete(UART_PORT);
        
        ESP_LOGI(TAG, "Booting STM32G030 into normal mode...");
        gpio_set_level(GPIO_STM32_BOOT0, 0);
        gpio_set_level(GPIO_STM32_NRESET, 0);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_STM32_NRESET, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (err!=ESP_OK)
    {
        ESP_LOGE(TAG, "Firmware flashing failed!");
        return ESP_FAIL;
    } else {
       
       
        return ESP_OK;
    }
    


}