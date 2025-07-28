/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#ifndef _WIFI_H
#define _WIFI_H


#include <stdio.h>

#include <stdint.h>
#include "esp_event.h"
#include "esp_wifi.h"

// static void wifi_wifi_event_handler(void* arg, esp_event_base_t event_base,
//                                     int32_t event_id, void* event_data);                               
esp_err_t wifi_connect_to_ap(void);
uint8_t wifi_ap_connection_failed();
uint8_t wifi_is_connected_to_ap();

// Returns the first 4 bytes of the mac address in HEX form as string.
esp_err_t wifi_get_trimmed_mac(char * str);
esp_err_t wifi_init();
int8_t wifi_get_rssi();

void wifi_get_ip(char * str);
esp_err_t wifi_print_ip();
/// @brief Starts the wifi. Stops it as well if it is already running and then restarts it according to the wifi mode setting.
/// @return ESP_OK if succesfull, ESP_FAIL if not.
esp_err_t wifi_disconnect_ap();
/// @brief Starts the wifi.
/// @return ESP_OK if succesfull, ESP_FAIL if not.
esp_err_t wifi_start();

#endif