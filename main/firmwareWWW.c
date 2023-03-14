#include "esp_system.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_sd_card.h"
#include "firmwareWWW.h"

#include "esp_vfs_semihost.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"

// Define the partition label and data bin file path
#define PARTITION_LABEL "www"
#define BIN_PATH "/sdcard/www.bin"

const char * TAG_WWW = "FW-WWW";

// Function to write the data bin file to flash memory
esp_err_t update_www() {


    // Unmount the spiffs partition
    ESP_LOGI(TAG_WWW, "Unmounting spiffs partition");
    esp_vfs_spiffs_unregister("www");

    // Mount the SD card
    ESP_LOGI(TAG_WWW, "Mounting SD card...");
    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_WWW, "Error mounting SD card.\n");
        return ESP_FAIL;
    }

    // Open the data bin file for reading
    ESP_LOGI(TAG_WWW, "Opening data bin file");

    FILE* fp = fopen(BIN_PATH, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG_WWW,"Error opening file\n");
        return ESP_FAIL;
    }

    // Get the partition object for the specified label
    ESP_LOGI( TAG_WWW, "Finding partition with label '%s'", PARTITION_LABEL);
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, PARTITION_LABEL);
    if (partition == NULL) {
        ESP_LOGE(TAG_WWW,"Partition not found\n");
        fclose(fp);
        return ESP_FAIL;
    }

    // Unmount the partition
    

    // Get the address of the first sector of the partition
    uint32_t flash_address = partition->address;

    // Erase the entire partition
    ESP_LOGI(TAG_WWW, "Erasing partition");
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_WWW,"Error erasing partition: %d\n", err);
        fclose(fp);
        return ESP_FAIL;
    }

    // Write the data bin file to flash memory
    size_t bytes_written = 0;
    uint8_t buf[1024];
    ESP_LOGI(TAG_WWW, "Writing data bin to flash memory");
    while (true) {
        // Read a chunk of data from the file
        size_t bytes_read = fread(buf, 1, sizeof(buf), fp);
        if (bytes_read == 0) {
            break; // End of file
        }

        // Write the data to flash memory
        err = esp_flash_write(NULL, (const void*)buf, flash_address + bytes_written,  bytes_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_WWW,"Error writing to flash memory: %d, %s\n", err, esp_err_to_name(err));
            
            fclose(fp);
            return ESP_FAIL;
        }

        bytes_written += bytes_read;
    }

    // Close the file
    fclose(fp);

    ESP_LOGI(TAG_WWW,"Data bin written to flash memory. Remounting parition\n");


    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = "www",
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    //unmount sd card
    esp_sd_card_unmount();

    if (ret!= ESP_FAIL)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}
