#pragma once

#include "esp_system.h"

static const char SW_VERSION[] =  "0.2.0_2023.09.06.16.37";

float sysinfo_get_core_temperature();
const char * sysinfo_get_fw_version();
esp_err_t sysinfo_init();
