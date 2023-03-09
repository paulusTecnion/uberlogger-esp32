#include "esp_system.h"
#include "esp_log.h"
#include "driver/temperature_sensor.h"
#include "sysinfo.h"

    temperature_sensor_handle_t temp_handle = NULL;


float sysinfo_get_core_temperature()
{
    // float t;

    // temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();

    // ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    // ESP_ERROR_CHECK(temp_sensor_start());
    // temp_sensor_read_celsius(&t);
    // ESP_ERROR_CHECK(temp_sensor_stop());
    // t = (int)(t * 100 + 0.5);
    
    // return (float)t/100;



   
    // Get converted sensor data
    float tsens_out = 20.0;
    // ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &tsens_out));
    return tsens_out;

}

esp_err_t sysinfo_init()
{    temperature_sensor_config_t temp_sensor = {
        .range_min = -10,
        .range_max = 80,
    };


    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
    return ESP_OK;
}

const char * sysinfo_get_fw_version()
{
    return SW_VERSION;
}

