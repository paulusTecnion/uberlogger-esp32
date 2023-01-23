#include "esp_system.h"
#include "esp_log.h"
#include "driver/temp_sensor.h"
#include "sysinfo.h"

float sysinfo_get_core_temperature()
{
    float t;
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    ESP_ERROR_CHECK(temp_sensor_start());
    temp_sensor_read_celsius(&t);
    ESP_ERROR_CHECK(temp_sensor_stop());
    t = (int)(t * 100 + 0.5);
    
    return (float)t/100;
    
}

const char * sysinfo_get_fw_version()
{
    return SW_VERSION;
}

