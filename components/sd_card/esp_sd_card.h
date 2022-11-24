#pragma once
void esp_sd_card_init();
int esp_sd_card_close_unmount(void);
int esp_sd_card_write(const void * data, size_t len);
int esp_sd_card_csv_write(const void * data, size_t len);