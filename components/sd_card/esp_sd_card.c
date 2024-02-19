#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "ff.h"
#include "vfs_fat_internal.h"

#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "esp_sd_card.h"
#include "../../main/config.h"
#include "../../main/logger.h"
#include "../../main/errorcodes.h"


#ifdef CONFIG_IDF_TARGET_ESP32
#include "driver/sdmmc_host.h"
#endif

static const char *TAG = "SDCARD";

#define MOUNT_POINT "/sdcard"

// This example can use SDMMC and SPI peripherals to communicate with SD card.
// By default, SDMMC peripheral is used.
// To enable SPI mode, uncomment the following line:

 #define USE_SPI_MODE

// ESP32-S2 doesn't have an SD Host peripheral, always use SPI:
#ifdef CONFIG_IDF_TARGET_ESP32S2
#ifndef USE_SPI_MODE
#define USE_SPI_MODE
#endif // USE_SPI_MODE
// on ESP32-S2, DMA channel must be the same as host id
#define SPI_DMA_CHAN    SDCARD_SPI_HOST
#endif //CONFIG_IDF_TARGET_ESP32S2

// DMA channel to be used by the SPI peripheral
#ifndef SPI_DMA_CHAN
#define SPI_DMA_CHAN    1
#endif //SPI_DMA_CHAN

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.


static sdmmc_card_t* card;
static const char mount_point[] = MOUNT_POINT;
sdmmc_host_t host;

sdspi_device_config_t slot_config ;
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 1,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = true
    };

static bool esp_sd_spi_is_initialized = false;
static bool esp_sd_card_is_mounted = false;
// Need to change this:
extern uint32_t _errorCode;


esp_err_t esp_sd_card_mount()
{
    if (!esp_sd_spi_is_initialized)
    {
        if (esp_sd_card_init() != ESP_OK)
            return ESP_FAIL;
    }

    if (esp_sd_card_check_for_card())
    {
        ESP_LOGE(TAG, "SD card not inserted!\n");
        return ESP_FAIL;
    }

  

    if (esp_sd_card_is_mounted)
    {
        #ifdef DEBUG_SDCARD
        ESP_LOGI(TAG, "Card already mounted");
        #endif
        return ESP_OK;
    }

    // Clear mount error, if any 
    CLEAR_BIT(_errorCode, ERR_LOGGER_SDCARD_UNABLE_TO_MOUNT);

    esp_err_t ret;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SDCARD_SPI_HOST;
    // host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    } else {
        esp_sd_card_is_mounted = true;
        return ESP_OK;
    }


}



esp_err_t esp_sd_card_init(void)
{
    esp_err_t ret= ESP_OK;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    
    #ifdef DEBUG_SDCARD
    ESP_LOGI(TAG, "Initializing SD card");
    #endif
    // gpio_set_direction(PIN_SD_CD, GPIO_MODE_INPUT);

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
#ifndef USE_SPI_MODE
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    // slot_config.width = 1;

    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
#else
    #ifdef DEBUG_SDCARD
    ESP_LOGI(TAG, "Using SPI peripheral");
    #endif
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SDCARD_SPI_HOST;

    // probe max speed
    // host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8192, // defaults to 4092 for DMA mode
        //.flags = SPI_TRANS_MODE_DIO | SPI_TRANS_MULTILINE_ADDR          
    };

    gpio_set_drive_capability(PIN_NUM_MOSI, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(PIN_NUM_CLK, GPIO_DRIVE_CAP_3);
    
    host.max_freq_khz = 50000;
    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDCARD_SPI_HOST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    } else {
        #ifdef DEBUG_SDCARD
        ESP_LOGI(TAG, "SPI bus initialized.");
        #endif
        esp_sd_spi_is_initialized = true;
    }
    
#endif //USE_SPI_MODE

    
    return ESP_OK;
    
}
/**
 * From: https://gist.github.com/dizcza/a35d8c1d09450369ed2f08f6803b5101
 * 
 * Usage:
 *   // See https://github.com/espressif/esp-idf/blob/b63ec47238fd6aa6eaa59f7ad3942cbdff5fcc1f/examples/storage/sd_card/sdmmc/main/sd_card_example_main.c#L75
 *   esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
 *   format_sdcard(card);
 *   // proceed without remounting
 */
esp_err_t esp_sd_card_format()
{

    if (esp_sd_card_check_for_card()) {
        ESP_LOGE("sdcard", "SD card not inserted!");
        return ESP_FAIL;
    }

    // check if it's mounted 
    if (!esp_sd_card_is_mounted) {
        if (esp_sd_card_mount() != ESP_OK)
        {
            return ESP_FAIL;
        }   
    }

	char drv[3] = {'0', ':', 0};
    const size_t workbuf_size = 4096;
    void* workbuf = NULL;
    esp_err_t err = ESP_OK;
    ESP_LOGW("sdcard", "Formatting the SD card");

    size_t allocation_unit_size = 16 * 1024;

    workbuf = ff_memalloc(workbuf_size);
    if (workbuf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                card->csd.sector_size,
                allocation_unit_size);

    const MKFS_PARM opt = {(BYTE)FM_ANY, 0, 0, 0, alloc_unit_size};
    FRESULT res = f_mkfs(drv, &opt, workbuf, workbuf_size);
    if (res != FR_OK) {
        err = ESP_FAIL;
        ESP_LOGE("sdcard", "f_mkfs failed (%d)", res);
    }

    free(workbuf);

    // ESP_LOGI("sdcard", "Successfully formatted the SD card");

    return err;
}

esp_err_t esp_sd_card_unmount(void)
{

    // Clear mount error, if any 
    CLEAR_BIT(_errorCode, ERR_LOGGER_SDCARD_UNABLE_TO_MOUNT);

    if (esp_sd_card_is_mounted)
    {
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    #ifdef DEBUG_SDCARD
    ESP_LOGI(TAG, "File closed and card unmounted");
    #endif
    esp_sd_card_is_mounted = false;
    return ESP_OK;
    } else {
    ESP_LOGE(TAG, "SD card not mounted!");
    return ESP_FAIL;
    }
    
}


uint8_t esp_sd_card_check_for_card(){
    return gpio_get_level(PIN_SD_CD);        
}

uint32_t esp_sd_card_get_free_space()
{
        if (!esp_sd_card_check_for_card())
        {
            if (!esp_sd_card_is_mounted && 
                (esp_sd_card_mount() != ESP_OK))
            {
                ESP_LOGE(TAG, "SD card cannot be mounted");
                return 0;
            }
                
            FATFS *fs;
            uint64_t fre_clust; //, tot_sect;
            DWORD fre_sect;
            /* Get volume information and free clusters of drive 0 */
            int res = f_getfree("/sdcard/", &fre_clust, &fs);
            if (res) {
                ESP_LOGE(TAG, "f_getfree failed (%d)", res);
                return 0;
            }
            /* Get total sectors and free sectors */
            // tot_sect = (fs->n_fatent - 2) * fs->csize;
            fre_sect = fre_clust * fs->csize;
            /* Print the free space (assuming 512 bytes/sector) */
            // ESP_LOGI(TAG, "%10u KiB total drive space.\r\n%10u KiB available.\r\n%10u free clust.\r\n",tot_sect / 2, fre_sect / 2,fre_clust);

            return (uint32_t)(fre_sect / 2);
        } else {
            ESP_LOGE(TAG, "SD card not available");
            return 0;
        }
}

uint8_t esp_sdcard_is_mounted()
{
    return esp_sd_card_is_mounted;
}

sdcard_state_t esp_sd_card_get_state()
{
    if (esp_sd_card_check_for_card())
    {
        return SDCARD_EJECTED;
    }

    if (esp_sd_card_is_mounted)
    {
        return SDCARD_MOUNTED;
    } else {
        return SDCARD_UNMOUNTED;
    }
}


