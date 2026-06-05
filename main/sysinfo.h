/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#pragma once

#include "esp_system.h"

static const char SW_VERSION[] =  "1.3.2_2026.06.05.20.21";

// float sysinfo_get_core_temperature();
const char * sysinfo_get_fw_version();
esp_err_t sysinfo_init();
