#pragma once

#include "esp_system.h"

static const char SW_VERSION[] =  "1.0.4_2024.03.27.08.14";

// float sysinfo_get_core_temperature();
const char * sysinfo_get_fw_version();
esp_err_t sysinfo_init();
