#pragma once

#include "esp_system.h"

static const char SW_VERSION[] =  "1.1.2_2024.03.28.17.53";

// float sysinfo_get_core_temperature();
const char * sysinfo_get_fw_version();
esp_err_t sysinfo_init();
