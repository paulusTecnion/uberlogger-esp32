#pragma once

#include "esp_system.h"

static const char SW_VERSION[] =  "1.2.0_2024.09.19.14.25";

// float sysinfo_get_core_temperature();
const char * sysinfo_get_fw_version();
esp_err_t sysinfo_init();
