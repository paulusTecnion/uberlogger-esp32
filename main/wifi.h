#ifndef _WIFI_H
#define _WIFI_H


#include <stdio.h>

#include <stdint.h>
#include "esp_event.h"
#include "esp_wifi.h"

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data);

void wifi_init_sta(void);

void wifi_init_softap(void);

#endif