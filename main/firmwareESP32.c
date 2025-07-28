/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "firmwareESP32.h"
#include "esp_sd_card.h"

const char *TAG = "FW-ESP32";

esp_ota_handle_t ota_handle;

esp_err_t updateESP32() {
  // Start OTA update process
  esp_err_t err = esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting OTA update: %d\n", err);
    return ESP_FAIL;
  }


  // Mount SD card
  ESP_LOGI(TAG, "Mounting SD card...");
  // if (esp_sd_card_mount() != ESP_OK) {
  //   ESP_LOGE(TAG, "Error mounting SD card.\n");
  //   return ESP_FAIL;
  // }

  // Open firmware file on SD card
  ESP_LOGI(TAG, "Opening firmware");
  FILE *file = fopen("/sdcard/ota_main.bin", "rb");
  if (!file) {
    ESP_LOGE(TAG, "Error opening firmware file.\n");
    return ESP_FAIL;
  }

  // Write firmware data to OTA partition
  const int buff_size = 2048;
  ESP_LOGI(TAG, "Writing firmware to OTA partition..."	);
  char *buffer = malloc(buff_size);
  while (1) {
    int bytes_read = fread(buffer, 1, buff_size, file);
    if (bytes_read == 0) {
      break;
    }
    esp_err_t err = esp_ota_write(ota_handle, buffer, bytes_read);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error writing firmware to OTA partition: %d\n", err);
      break;
    }
  }
  free(buffer);

  // Close firmware file
  fclose(file);

  // End OTA update process
  err = esp_ota_end(ota_handle);

  // Unmount SD card
  // esp_sd_card_unmount();
  
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error ending OTA update: %d\n", err);
    return ESP_FAIL;
  }

  // Set OTA boot partition
  err = esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting OTA boot partition: %d\n", err);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Firmware update complete. Rebooting...\n");


  return ESP_OK;
}
