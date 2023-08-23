#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
// #include "u8g2.h"

#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "config.h"
#include "sdkconfig.h"
#include "esp_netif.h"
// #include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "protocol_examples_common.h"

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "settings.h"
#include "wifi.h"

/* The examples use WiFi configuration that you can set via project configuration menu.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN


#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_EXAMPLE_TASK_STACK_SIZE)


#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

static const char * TAG = "WIFI";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

esp_netif_t *sta_netif;
esp_netif_t *ap_netif;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define CONNECTED_BIT BIT0
#define FAIL_BIT      BIT1



static int s_retry_num = 0;
static uint8_t wifi_enabled = 0;
static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && 
        event_id == WIFI_EVENT_STA_DISCONNECTED && 
        s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
		ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
        // Only when in station mode, we try to reconnect
        // Else when changing from STA to APSTA mode,
        // we keep connecting after a disconnect.
		if (settings_get_wifi_mode() == WIFI_MODE_APSTA) 
        {
            esp_wifi_connect();
        }
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT | FAIL_BIT);

        s_retry_num++;
        if (s_retry_num >= EXAMPLE_ESP_MAXIMUM_RETRY) 
        {
            xEventGroupSetBits(wifi_event_group, FAIL_BIT);
        }
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	}
}


uint8_t wifi_ap_connection_failed()
{
    return xEventGroupGetBits(wifi_event_group) & FAIL_BIT;
}

uint8_t wifi_is_connected_to_ap()
{
    return xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT;
}

esp_err_t wifi_disconnect_ap()
{
    // disconnect from AP 
    esp_err_t err = esp_wifi_disconnect();
    return err;
}

esp_err_t wifi_connect_to_ap(void)
{
    if (wifi_disconnect_ap() != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to disconnect from AP");
    }


    wifi_config_t wifi_config = {
        .sta = {
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
	     * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            //.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .threshold.rssi = -80,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .scan_method = WIFI_ALL_CHANNEL_SCAN, // scan all channels for potentially same SSID on a better channel
            .failure_retry_cnt =3,                  // retry 3 times in case of failure. Potentially remove own retry system? 
        },
    };
    s_retry_num = 0;
    strcpy((char*)wifi_config.sta.ssid, settings_get_wifi_ssid());
    
    // wifi_config.sta.ssid_len = strlen((const char*)(wifi_config.sta.ssid));
    strcpy((char*)wifi_config.sta.password, settings_get_wifi_password());

    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) );
    
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    // ESP_ERROR_CHECK(esp_wifi_start() );
    
    esp_wifi_connect();
    #ifdef DEBUG_WIFI
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    #endif

    
      /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    // EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
    //         CONNECTED_BIT | FAIL_BIT,
    //         pdFALSE,
    //         pdFALSE,
    //         portMAX_DELAY);

    // /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
    //  * happened. */
    // if (bits & CONNECTED_BIT) {
    //     #ifdef DEBUG_WIFI
    //     ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
    //              EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    //     #endif
        
    // } else if (bits & FAIL_BIT) {
    //     #ifdef DEBUG_WIFI
    //     ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
    //              EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    //     #endif
    //     return ESP_FAIL;
    // } else {
    //     ESP_LOGE(TAG, "UNEXPECTED EVENT");
    //     return ESP_FAIL;
    // }
   
    return ESP_OK;
}


esp_err_t wifi_get_trimmed_mac(char * str)
{
    uint8_t mac[6];
    
    if (esp_wifi_get_mac(WIFI_IF_AP, mac) == ESP_OK)
    {
        sprintf(str, "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }

    
}


esp_err_t wifi_init()
{
     esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("wifi", ESP_LOG_WARN);

	ESP_ERROR_CHECK(esp_netif_init());
	wifi_event_group = xEventGroupCreate();


	// ESP_ERROR_CHECK(esp_event_loop_create_default());
	ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);
	sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL) );
    

	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );

    
                                                        
    return ESP_OK;
    
}


esp_err_t wifi_start()
{
    wifi_config_t wifi_config = {
        .ap = {
            // .ssid = wifi_ssid,
            // .ssid_len = strlen(wifi_ssid),
            .channel = settings_get_wifi_channel(),
            // .password = wifi_pass,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
        // .sta = {
        //     /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
        //      * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
        //      * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
	    //  * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
        //      */
        //     .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        //     .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        // },
    };

    // always default to Uberlogger
    strcpy((char*)wifi_config.ap.ssid, settings_get_wifi_ssid_ap());
    wifi_config.ap.ssid_len = strlen((const char*)(wifi_config.ap.ssid));
    strcpy((char*)wifi_config.ap.password, "");

    if (strlen((const char*)(wifi_config.ap.password)) == 0) {
        wifi_config.ap.authmode =  WIFI_AUTH_OPEN;
    }

    // strcpy((char*)wifi_config.sta.ssid, settings_get_wifi_ssid());
    
    // wifi_config.sta.ssid_len = strlen((const char*)(wifi_config.sta.ssid));
    // strcpy((char*)wifi_config.sta.password, settings_get_wifi_password());
    // Enable to Access Point and Station mode
    // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    
    #ifdef DEBUG_WIFI
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, EXAMPLE_ESP_WIFI_CHANNEL);
    #endif


    // esp_wifi_set_max_tx_power(60); // corresponding to 15 dBi

	ESP_ERROR_CHECK( esp_wifi_start() );

    wifi_enabled = 1;

     if (settings_get_wifi_mode() == WIFI_MODE_APSTA) {
        #ifdef DEBUG_WIFI
        ESP_LOGI(TAG, "Connecting with AP");
        #endif
        wifi_connect_to_ap();
     }

     return ESP_OK;

}

int8_t wifi_get_rssi()
{
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    return ap_info.rssi;
}

void wifi_get_ip(char * str)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(sta_netif, &ip_info);
    sprintf(str, IPSTR, IP2STR(&ip_info.ip));
    
}

esp_err_t wifi_print_ip()
{
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(sta_netif, &ip_info);
    ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
    ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    return ESP_OK;
}




// To do: split up start / stop functions to event handlers for wifi. 