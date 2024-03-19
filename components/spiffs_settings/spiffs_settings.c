
#include "spiffs_settings.h"
#include "esp_spiffs.h"


static const char *TAG = "SPIFFS";

#define SETTINGS_BASE_PATH  "/data"



FILE * f;
char _filename[30];

esp_err_t spiffs_init()
{
    ESP_LOGI(TAG, "Initializing SPIFFS using");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "settings",
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
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

#ifdef CONFIG_EXAMPLE_SPIFFS_CHECK_ON_START
    ESP_LOGI(TAG, "Performing SPIFFS_check().");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        return;
    } else {
        ESP_LOGI(TAG, "SPIFFS_check() successful");
    }
#endif

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }


    // Check consistency of reported partiton size info.
    if (used > total) {
        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "SPIFFS_check() successful");
        }
    }

    // if (strlen(filename_settings) < 22)
    // {
    //     strcpy(_filename, filename_settings);        
    // } else {
    //     return ESP_FAIL;
    // }


    return ESP_OK;   
}


esp_err_t spiffs_write(const char* data, size_t length, const char * filename)
{
    size_t writeSize = 0;
    char buffer[30];
    sprintf(buffer, "/spiffs/%s" ,filename);
    
    //  esp_vfs_spiffs_conf_t conf = {
    //   .base_path = SETTINGS_BASE_PATH,
    //   .partition_label = SPIFFS_LABEL,
    //   .max_files = 5,
    //   .format_if_mount_failed = false
    // };

   
    f = fopen(buffer, "w");
    if (!f)
    {
        ESP_LOGE(TAG, "Cannot open file for write: %d", f);
        return ESP_FAIL;
    }

    writeSize = fwrite((void*)data, 1, length,  f);
    if (ferror(f))
    {
        perror("Error: ");
    }
    
    if (writeSize != length)
    {
        ESP_LOGE(TAG, "Error writing file, bytes written %d, expected %d", writeSize, length);
        return ESP_FAIL;
    }
    
   fclose(f);
    return ESP_OK;
}

esp_err_t spiffs_read(char* data, size_t length, const char * filename)
{
    size_t readSize = 0;
    char buffer[30];
    sprintf(buffer, "/spiffs/%s", filename);
    
    //  esp_vfs_spiffs_conf_t conf = {
    //   .base_path = SETTINGS_BASE_PATH,
    //   .partition_label = SPIFFS_LABEL,
    //   .max_files = 5,
    //   .format_if_mount_failed = false
    // };

   
    f = fopen(buffer, "r");
    if (!f)
    {
        ESP_LOGE(TAG, "Cannot open file for read");
        return ESP_FAIL;
    }

    readSize = fread((void*)data, 1, length,  f);
    if (ferror(f))
    {
        perror("Error: ");
    }
    
    if (readSize != length)
    {
        ESP_LOGE(TAG, "Error reading file, bytes read %d, expected %d", readSize, length);
        return ESP_FAIL;
    }
    
   fclose(f);
    return ESP_OK;
}


esp_err_t spiffs_delete(const char * filename)
{
    char buffer[50];
    sprintf(buffer, "/spiffs/%s", filename);

    if (unlink(buffer) == 0) {
        ESP_LOGI("SPIFFS", "File deleted successfully");
        return ESP_OK;
    } else {
        ESP_LOGE("SPIFFS", "Failed to delete file");
        return ESP_FAIL;
    }
}
