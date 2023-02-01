#pragma once

typedef enum sdcard_state_e  {
    SDCARD_EJECTED = 0,
    SDCARD_UNMOUNTED,
    SDCARD_MOUNTED
} sdcard_state_t;

esp_err_t esp_sd_card_init();
esp_err_t esp_sd_card_unmount(void);
esp_err_t esp_sd_card_mount(void);

// esp_sd_card_check_for_card() returns 0 when card detected and 1 when none is detected.
uint8_t esp_sd_card_check_for_card();
uint32_t esp_sd_card_get_free_space(void);
sdcard_state_t esp_sd_card_get_state(void);