#ifndef _WIFI_H
#define _WIFI_H


#include <stdio.h>

#include <stdint.h>
#include "esp_event.h"
#include "esp_wifi.h"

// static void wifi_event_handler(void* arg, esp_event_base_t event_base,
//                                     int32_t event_id, void* event_data);

esp_err_t wifi_connect_to_ap(void);
esp_err_t wifi_is_connected_to_ap();

// Returns the first 4 bytes of the mac address in HEX form as string.
esp_err_t wifi_get_trimmed_mac(char * str);
esp_err_t wifi_init();
int8_t wifi_get_rssi();
esp_err_t wifi_print_ip();
/// @brief Starts the wifi. Stops it as well if it is already running and then restarts it according to the wifi mode setting.
/// @return ESP_OK if succesfull, ESP_FAIL if not.
esp_err_t wifi_disconnect_ap();

#endif