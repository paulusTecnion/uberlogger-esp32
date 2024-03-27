#include "esp_system.h"
#include "esp_log.h"
#include "config.h"
#include "driver/gpio.h"
#include <string.h>
#include "sysinfo.h"

static char sw_hw_version[50];

esp_err_t sysinfo_init()
{    
    // Get hardware version 
    gpio_set_direction(BOARD_REV0, GPIO_MODE_INPUT);
    gpio_set_direction(BOARD_REV1, GPIO_MODE_INPUT);
    gpio_set_direction(BOARD_REV2, GPIO_MODE_INPUT);

  uint8_t hwversion = 0;
  hwversion = (gpio_get_level(BOARD_REV0)) | (( gpio_get_level(BOARD_REV1) << 1 )) | ((gpio_get_level(BOARD_REV2)<<2) );

   if (hwversion == 7)
  {
    sprintf(sw_hw_version, "%s%s", SW_VERSION, "R00");
  } else if (hwversion == 6) {
    sprintf(sw_hw_version, "%s%s", SW_VERSION, "R04");
  } else if (hwversion == 5){
    sprintf(sw_hw_version, "%s%s", SW_VERSION, "R05");
  }

    return ESP_OK;
}

const char * sysinfo_get_fw_version()
{
    return sw_hw_version;
}

