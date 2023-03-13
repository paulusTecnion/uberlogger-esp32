#include <stdio.h>
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"



esp_ota_handle_t ota_handle;

esp_err_t updateESP32() {
  // Start OTA update process
//   esp_err_t err = esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &ota_handle);
//   if (err != ESP_OK) {
//     printf("Error starting OTA update: %d\n", err);
//     return ESP_FAIL;
//   }

//   // Open firmware file on SD card
//   FILE *file = fopen("/sdcard/firmware.bin", "rb");
//   if (!file) {
//     printf("Error opening firmware file.\n");
//     return ESP_FAIL;
//   }

//   // Write firmware data to OTA partition
//   const int buff_size = 1024;
//   char *buffer = malloc(buff_size);
//   while (1) {
//     int bytes_read = fread(buffer, 1, buff_size, file);
//     if (bytes_read == 0) {
//       break;
//     }
//     esp_err_t err = esp_ota_write(ota_handle, buffer, bytes_read);
//     if (err != ESP_OK) {
//       printf("Error writing firmware to OTA partition: %d\n", err);
//       break;
//     }
//   }
//   free(buffer);

//   // Close firmware file
//   fclose(file);

//   // End OTA update process
//   err = esp_ota_end(ota_handle);
//   if (err != ESP_OK) {
//     printf("Error ending OTA update: %d\n", err);
//     return;
//   }

  // Set OTA boot partition
//   err = esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
//   if (err != ESP_OK) {
//     printf("Error setting OTA boot partition: %d\n", err);
//     return;
//   }

//   // Reboot to apply firmware update
//   esp_restart();

return ESP_OK;
}
