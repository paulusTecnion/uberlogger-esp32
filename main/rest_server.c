/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>

#include "file_server.h"
#include "rest_server.h"
#include "config.h"
#include "wifi.h"
#include "esp_wifi_types.h"

char * endpoint_response_char[] = 
{
    "OK",
    "NACK",
    "ERROR"
};


converted_reading_t live_data;

static const char *REST_TAG = "esp-rest";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

httpd_handle_t server = NULL;

// extern SemaphoreHandle_t sdcard_semaphore;

static const char *UPLOAD_FORM = "<html><body>\
<form method='POST' enctype='multipart/form-data' action='/upload'>\
<input type='file' name='file'><br>\
<input type='submit' value='Upload'><br>\
</form></body></html>";

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript"; // Corrected from application/javscript
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "image/svg+xml"; // Corrected from text/xml to the more specific image/svg+xml
    } else if (CHECK_FILE_EXTENSION(filepath, ".gz")) {
        type = "application/gzip"; // Corrected to application/gzip, noting this is for gzip compressed files
    } 

    
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    // char * buf;
    size_t buf_len = httpd_req_get_url_query_len(req);
    #ifdef DEBUG_REST_SERVER
    ESP_LOGI(REST_TAG, "Query length %d, filepath %d", buf_len, strlen(filepath));
    #endif

    
    if (buf_len > 0)
    {
        strlcpy(filepath, filepath, strlen(filepath)-buf_len);
    }
    // ESP_LOGI("REST", "URI: %s", filepath);
    if (strncmp(filepath, rest_context->base_path, strlen(filepath) - 1) == 0)
    {
        strlcat(filepath, "index.html", sizeof(filepath));
    }

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        // ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        if (CHECK_FILE_EXTENSION(filepath, ".js"))
        {
            strlcat(filepath, ".gz", sizeof(filepath));
        }
        // ESP_LOGE(REST_TAG, "Trying %s", filepath);
        fd = open(filepath, O_RDONLY, 0);

        if (fd == -1)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
            return ESP_FAIL;
        }
    }

    set_content_type_from_file(req, filepath);

    if (CHECK_FILE_EXTENSION(filepath, ".gz"))
    {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    if (CHECK_FILE_EXTENSION(filepath, ".json")){
            httpd_resp_set_type(req, "text/json");
    }

    if (CHECK_FILE_EXTENSION(filepath, ".py") )
    {
         // Extract the filename from the filepath
        const char *filename = strrchr(filepath, '/');
        if (filename) // Check if the '/' character was found
        {
            filename++; // Move past the '/' to the start of the actual filename
        }
        else
        {
            filename = filepath; // No '/' found, the whole path is the filename
        }

        // Set the HTTP headers
        httpd_resp_set_hdr(req, "Content-Type", "application/octet-stream");

        // Create a buffer for the Content-Disposition header
        char content_disposition[256]; // Adjust size as needed
        snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s\"", filename);

        // Set the Content-Disposition header with the actual filename
        httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);
    }

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            // ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    #ifdef DEBUG_REST_SERVER
    ESP_LOGI(REST_TAG, "File sending complete");
    #endif
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t logger_getValues_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON * root = cJSON_CreateObject();
    if (root == NULL)
    {
        return ESP_FAIL;
    }
     
    cJSON_AddNumberToObject(root, "TIMESTAMP", live_data.timestamp);
    
    cJSON *readings = cJSON_AddObjectToObject(root, "READINGS");

    cJSON *temperature = cJSON_AddObjectToObject(readings, "TEMPERATURE");
    cJSON_AddStringToObject(temperature, "UNITS", "DEG C");
    cJSON *tValues = cJSON_AddObjectToObject(temperature, "VALUES");

    cJSON *analog = cJSON_AddObjectToObject(readings, "ANALOG");
    cJSON_AddStringToObject(analog, "UNITS", "Volt");
    cJSON * aValues = cJSON_AddObjectToObject(analog, "VALUES");
    
    char buf[15];
    for (int i=ADC_CHANNEL_0; i<=ADC_CHANNEL_7; i++)
    {
        if (settings_get_adc_channel_enabled(i))
        {
            if (settings_get_adc_channel_type(settings_get(), i))
            {
                sprintf(buf,"T%d", i+1);
                
                cJSON_AddNumberToObject(tValues, buf, (double)live_data.temperatureData[i]/10.0);
            } else {
                sprintf(buf,"AIN%d", i+1);
                
                 if (settings_get_adc_channel_range(settings_get(), i))
                {
                    cJSON_AddNumberToObject(aValues, buf, (double)live_data.analogData[i] / (double)ADC_MULT_FACTOR_60V);
                } else {
                    cJSON_AddNumberToObject(aValues, buf, (double)live_data.analogData[i] / (double)ADC_MULT_FACTOR_10V);
                // cJSON_AddStringToObject(aValues, buf, live_data.analogData[i]);
                }
            }
        }
        
        
    }
      
   
    // // For future use. Always enabled now.
    // // cJSON_AddNumberToObject(root, "adc_channels_enabled", settings->adc_channels_enabled);
    
   cJSON *digital = cJSON_AddObjectToObject(readings, "DIGITAL");
   cJSON_AddStringToObject(digital, "UNITS", "Level");
//    cJSON *dValues = cJSON_AddObjectToObject(readings, "VALUES");
    cJSON * aDigital = cJSON_AddObjectToObject(digital, "VALUES");


    for (int i = 0; i<6; i++)
    {
        if (settings_get_gpio_channel_enabled(settings_get(), i))
        {
            sprintf(buf, "DI%d", i+1);
            cJSON_AddNumberToObject(aDigital, buf, live_data.gpioData[i]);
        }
            
    }
    
    // cJSON_AddNumberToObject(aDigital, "DI2", live_data.gpioData[1]);
    // cJSON_AddNumberToObject(aDigital, "DI3", live_data.gpioData[2]);
    // cJSON_AddNumberToObject(aDigital, "DI4", live_data.gpioData[3]);
    // cJSON_AddNumberToObject(aDigital, "DI5", live_data.gpioData[4]);
    // cJSON_AddNumberToObject(aDigital, "DI6", live_data.gpioData[5]);

    // cJSON_AddNumberToObject(root, "TIMESTAMP", live_data.timestamp);
    cJSON_AddNumberToObject(root, "LOGGER_STATE", Logger_getState());
    cJSON_AddNumberToObject(root, "ERRORCODE", Logger_getError());
    // cJSON_AddNumberToObject(root, "T_CHIP", sysinfo_get_core_temperature());
    cJSON_AddStringToObject(root, "FW_VERSION", sysinfo_get_fw_version());
    cJSON_AddNumberToObject(root, "SD_CARD_FREE_SPACE", Logger_getLastFreeSpace());

    cJSON_AddNumberToObject(root, "SD_CARD_STATUS", esp_sd_card_get_state());
    
    uint8_t wifi_state = 0;
    if (settings_get_wifi_mode() == WIFI_MODE_APSTA)
    {
        if (wifi_is_connected_to_ap())
        {
            wifi_state = 3;

        } else if (wifi_ap_connection_failed())
        {
            wifi_state = 2;
        } else {
            wifi_state = 1;
        }
    } 
    
    cJSON_AddNumberToObject(root, "WIFI_TEST_STATUS", wifi_state);
    
    if (wifi_is_connected_to_ap())
    {
        char buffer[20];
        wifi_get_ip(buffer);
        cJSON_AddStringToObject(root, "WIFI_TEST_IP", buffer);
        cJSON_AddNumberToObject(root, "WIFI_TEST_RSSI", wifi_get_rssi());
    } else {
        cJSON_AddStringToObject(root, "WIFI_TEST_IP", "0.0.0.0");
        cJSON_AddNumberToObject(root, "WIFI_TEST_RSSI", 0);
    }

    
    const char *settings_json= cJSON_Print(root);

    // if (xSemaphoreTake(sdcard_semaphore, portMAX_DELAY) != pdTRUE)
    // {
    //     ESP_LOGE(REST_TAG, "Failed to take semaphore");
    //     return ESP_FAIL;
    // } else {
        httpd_resp_sendstr(req, settings_json);
        // xSemaphoreGive(sdcard_semaphore);
    // }

    
    
    
    free((void *)settings_json);
    cJSON_Delete(root);
    
    return ESP_OK;
    
}

static esp_err_t logger_getRawAdc_handler(httpd_req_t *req)
{
     httpd_resp_set_type(req, "application/json");
    cJSON * root = cJSON_CreateObject();
    if (root == NULL)
    {
        return ESP_FAIL;
    }

    char buf[5];

    for (int i=ADC_CHANNEL_0; i<=ADC_CHANNEL_7; i++)
    {
            sprintf(buf,"AIN%d", i+1);
            cJSON_AddNumberToObject(root, buf, live_data.analogDataRaw[i]);   
    }


    const char *settings_json= cJSON_Print(root);
    httpd_resp_sendstr(req, settings_json);
    free((void *)settings_json);

    cJSON_Delete(root);
    
    return ESP_OK;

   
   
}

static esp_err_t logger_getStatus_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON * root = cJSON_CreateObject();
    if (root == NULL)
    {
        return ESP_FAIL;
    }
   
    cJSON_AddNumberToObject(root, "LOGGER_STATE", Logger_getState());
    cJSON_AddNumberToObject(root, "ERRORCODE", Logger_getError());
    // cJSON_AddNumberToObject(root, "T_CHIP", sysinfo_get_core_temperature());
    cJSON_AddStringToObject(root, "FW_VERSION", sysinfo_get_fw_version());
    
    cJSON_AddNumberToObject(root, "SD_CARD_FREE_SPACE", Logger_getLastFreeSpace());
    
    cJSON_AddNumberToObject(root, "SD_CARD_STATUS", esp_sd_card_get_state());

     uint8_t wifi_state = 0;
    if (settings_get_wifi_mode() == WIFI_MODE_APSTA)
    {
        if (wifi_is_connected_to_ap())
        {
            // connected
            wifi_state = 3;

        } else if (wifi_ap_connection_failed())
        {
            // Connection failed
            wifi_state = 2;
        } else {
            // Connecting...
            wifi_state = 1;
        }
    } 
    
    cJSON_AddNumberToObject(root, "WIFI_TEST_STATUS", wifi_state);
    
    if (wifi_is_connected_to_ap())
    {
        char buffer[20];
        wifi_get_ip(buffer);
        cJSON_AddStringToObject(root, "WIFI_TEST_IP", buffer);
        cJSON_AddNumberToObject(root, "WIFI_TEST_RSSI", wifi_get_rssi());
    } else {
        cJSON_AddStringToObject(root, "WIFI_TEST_IP", "0.0.0.0");
        cJSON_AddNumberToObject(root, "WIFI_TEST_RSSI", 0);
    }

    const char *settings_json= cJSON_Print(root);
    httpd_resp_sendstr(req, settings_json);
    free((void *)settings_json);
    
    cJSON_Delete(root);
    return ESP_OK;
}


const char * logger_settings_to_json(Settings_t *settings)
{
    cJSON * root = cJSON_CreateObject();
    const char * strptr = NULL;
    if (root == NULL)
    {
        return strptr;
    }
  
    
    cJSON *range_select = cJSON_AddObjectToObject(root, "AIN_RANGE_SELECT");
    cJSON *ntc_select = cJSON_AddObjectToObject(root, "NTC_SELECT");
    cJSON *ain_enabled = cJSON_AddObjectToObject(root, "AIN_ENABLED");
    cJSON *din_enabled = cJSON_AddObjectToObject(root, "DIN_ENABLED");
    char buf[15];

    for (int i=ADC_CHANNEL_0; i<=ADC_CHANNEL_7; i++)
    {

            sprintf(buf,"NTC%d", i+1);
            cJSON_AddBoolToObject(ntc_select, buf, settings_get_adc_channel_type(settings, i));

            sprintf(buf,"AIN%d_RANGE", i+1);
            cJSON_AddBoolToObject(range_select, buf, settings_get_adc_channel_range(settings, i));
            
            sprintf(buf,"AIN%d_ENABLE", i+1);
            cJSON_AddBoolToObject(ain_enabled, buf, settings_get_adc_channel_enabled(i));
            
            if (i<6)
            {
                sprintf(buf,"DIN%d_ENABLE", i+1);
                cJSON_AddBoolToObject(din_enabled, buf, settings_get_gpio_channel_enabled(settings, i));
            }
    }

    cJSON_AddNumberToObject(root, "FILE_DECIMAL_CHAR", settings->file_decimal_char);
    cJSON_AddNumberToObject(root, "FILE_NAME_MODE", settings->file_name_mode);
    cJSON_AddStringToObject(root, "FILE_NAME_PREFIX", settings->file_prefix);
    cJSON_AddNumberToObject(root, "FILE_SEPARATOR_CHAR", settings->file_separator_char);
    cJSON_AddNumberToObject(root, "FILE_SPLIT_SIZE_UNIT", settings->file_split_size_unit);

    uint32_t file_split_size;

    switch (settings->file_split_size_unit)
    {
        default:
        case 0:
            file_split_size = settings->file_split_size / 1024;
        break;

        case 1:
            file_split_size = settings->file_split_size / (1024*1024);
        break;

        case 2:
            file_split_size = settings->file_split_size / (1024 * 1024 * 1024);
        break;
    }
    cJSON_AddNumberToObject(root, "FILE_SPLIT_SIZE", file_split_size );

    
    cJSON_AddStringToObject(root, "WIFI_SSID", settings->wifi_ssid);
    cJSON_AddNumberToObject(root, "WIFI_CHANNEL", settings->wifi_channel);
    cJSON_AddStringToObject(root, "WIFI_PASSWORD", settings->wifi_password);
    if (settings->wifi_mode == WIFI_MODE_AP)
    {
        cJSON_AddNumberToObject(root, "WIFI_MODE", 0);
    } else if (settings->wifi_mode == WIFI_MODE_APSTA)
    {
        cJSON_AddNumberToObject(root, "WIFI_MODE", 1);
    }
    

    cJSON_AddNumberToObject(root, "ADC_RESOLUTION", settings->adc_resolution);
    cJSON_AddNumberToObject(root, "LOG_SAMPLE_RATE", settings->adc_log_sample_rate);
    cJSON_AddNumberToObject(root, "LOG_MODE", settings->logMode);


    
    strptr = cJSON_Print(root);
    cJSON_Delete(root);

    return strptr;
}

static esp_err_t logger_calibrate_handler(httpd_req_t *req)
{   
    httpd_resp_set_type(req, "application/json");
    if (Logger_getState() == LOGTASK_LOGGING)
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Calibration cannot be started while logging.", HTTPD_403_FORBIDDEN); 
        return ESP_FAIL;
    } 
    else if (Logger_getState() == LOGTASK_CALIBRATION) 
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Calibration already started.", HTTPD_403_FORBIDDEN); 
        return ESP_FAIL;
    } else if ((Logger_getState() != LOGTASK_IDLE) && (Logger_getState() !=LOGTASK_SINGLE_SHOT))
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Logger not idle. Are you updating firmware?", HTTPD_403_FORBIDDEN); 
        return ESP_FAIL;
    }

    if (Logger_calibrate() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK, "Calibration started...", 0);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Calibration cannot be started. Are you logging?", HTTPD_403_FORBIDDEN);
        return ESP_FAIL;
    }

    return ESP_OK;
}


static esp_err_t logger_formatSdcard_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    if (Logger_format_sdcard() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK, "Formt started...", 0);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Could not start format. Do you have an sd card inserted or are you logging?", HTTPD_403_FORBIDDEN);
    }

    return ESP_OK;

}

static esp_err_t logger_getDefaultConfig(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    Settings_t settings = settings_get_default();
    const char * settings_json = NULL;
    settings_json = logger_settings_to_json(&settings);
    if (settings_json == NULL)
    {
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, settings_json);
    httpd_resp_sendstr_chunk(req, NULL);
    free((void *)settings_json);
    
    return ESP_OK;
}

static esp_err_t logger_getConfig_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    
    // Check if the query parameter for download is present and set to 'true'
    char query_param[20];  // Adjust size according to your expected query length
    if (httpd_req_get_url_query_str(req, query_param, sizeof(query_param)) == ESP_OK) {
        char param_value[5];  // Assuming the value is 'true' or 'false'
        if (httpd_query_key_value(query_param, "download", param_value, sizeof(param_value)) == ESP_OK) {
            if (strcasecmp(param_value, "true") == 0) {
                // The client wants to download the file, set the Content-Disposition header
                httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"settings.json\"");
            }
        }
    }

    Settings_t *settings = settings_get();
    const char *settings_json = NULL;
    settings_json = logger_settings_to_json(settings);
    if (settings_json == NULL) {
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, settings_json);
    httpd_resp_sendstr_chunk(req, NULL);
    free((void *)settings_json);
    
    return ESP_OK;
}

static esp_err_t logger_setTime_handler(httpd_req_t *req)
{
     httpd_resp_set_type(req, "application/json");
    cJSON *settings_in = NULL;
    if (Logger_getState() == LOGTASK_LOGGING)
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Cannot set settings while logging", HTTPD_403_FORBIDDEN);
        
        return ESP_OK;
    }
    cJSON * root = cJSON_CreateObject();
    if (root == NULL)
    {
        return ESP_FAIL;
    }



    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        // return ESP_FAIL;
        goto error2;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            // return ESP_FAIL;
            goto error2;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    settings_in = cJSON_Parse(buf);
    cJSON * item;

 

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "TIMESTAMP");
    
    if (item != NULL)
    {
        if (settings_set_timestamp((uint64_t)item->valuedouble) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Timestamp missing or wrong value", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

 

    if (Logtask_sync_time() == ESP_FAIL)
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Error storing settings", HTTPD_500_INTERNAL_SERVER_ERROR);
        // return ESP_FAIL;
        goto error;
    }


    // only send ack in case wifi mode has not changed. Else the next will get stuck
    json_send_resp(req, ENDPOINT_RESP_ACK, NULL, 0);
    

    
    

error:
    free((void*)settings_in);
error2:
    cJSON_Delete(root);
    

    return ESP_OK;
}


static esp_err_t logger_setConfig_handler(httpd_req_t *req)
{

    httpd_resp_set_type(req, "application/json");
    cJSON *settings_in = NULL;
    if (Logger_getState() == LOGTASK_LOGGING)
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Cannot set settings while logging", HTTPD_403_FORBIDDEN);
        
        return ESP_OK;
    }
    cJSON * root = cJSON_CreateObject();
    if (root == NULL)
    {
        return ESP_FAIL;
    }


    // Store old settings (only to compare old wifi settings)
    Settings_t oldSettings;
    memcpy(&oldSettings, settings_get(), sizeof(Settings_t));

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        // return ESP_FAIL;
        goto error2;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            // return ESP_FAIL;
            goto error2;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    settings_in = cJSON_Parse(buf);
    cJSON * item = NULL;

    
    char buf2[30];
    for (int j = 0; j<4; j++)
    {
        for (int i = 0; i<8; i++)
        {
            if (j == 0)
            {
                item = cJSON_GetObjectItemCaseSensitive(settings_in, "NTC_SELECT");
                if (item != NULL)
                {
                    sprintf(buf2, "NTC%d", i+1);
                }

            } else if (j == 1) {
                item = cJSON_GetObjectItemCaseSensitive(settings_in, "AIN_RANGE_SELECT");
                if (item != NULL)
                {
                    sprintf(buf2, "AIN%d_RANGE", i+1);
                }
            } else if (j == 2) {
                item = cJSON_GetObjectItemCaseSensitive(settings_in, "AIN_ENABLED");
                if (item != NULL)
                {
                    sprintf(buf2, "AIN%d_ENABLE", i+1);
                }
            } else if (j == 3) {
                if (i < 6)
                {
                    item = cJSON_GetObjectItemCaseSensitive(settings_in, "DIN_ENABLED");
                    if (item != NULL)
                    {
                        sprintf(buf2, "DIN%d_ENABLE", i+1);
                    }
                }
            } else {
                item = NULL;
            }
                
            if (item!=NULL)
            {
                cJSON * subItem = cJSON_GetObjectItemCaseSensitive(item, buf2);
                if (subItem != NULL)
                {
                    if (j == 0) {
                        #ifdef DEBUG_REST_SERVER
                        ESP_LOGI("REST: ", "NTC%d %d", i, (subItem->valueint));
                        #endif
                        settings_set_adc_channel_type(i, subItem->valueint);
                    } else if (j == 1) {
                        // #ifdef DEBUG_REST_SERVER
                        ESP_LOGI("REST: ", "AIN%d %d", i, (subItem->valueint));
                        // #endif
                        settings_set_adc_channel_range(i, subItem->valueint);
                    } else if (j == 2) {
                        ESP_LOGI("REST: ", "AIN ENABLE AIN%d %d", i, (subItem->valueint));
                        settings_set_adc_channel_enabled(i, subItem->valueint);
                    } else if (j == 3 ) {
                        ESP_LOGI("REST: ", "DIO ENABLE DI%d %d", i, (subItem->valueint));
                        settings_set_gpio_channel_enabled(i, subItem->valueint);
                    } 
                }
            }        
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "FILE_DECIMAL_CHAR");
    if (item != NULL)
    {
        if (settings_set_file_decimal_char(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Invalid file decimal char value. Only 0=dot and 1=comma possible.", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }
        
    item = cJSON_GetObjectItemCaseSensitive(settings_in, "FILE_NAME_MODE");
    if (item != NULL)
    {
        if (settings_set_file_name_mode(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Invalid file name mode value. Only 0=sequential and 1=timestamp possible.", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

    

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "FILE_NAME_PREFIX");
    if (item != NULL)
    {
        if (strcmp(item->valuestring, "") == 0)
        {
            json_send_resp(req, ENDPOINT_RESP_NACK, "Filename prefix cannot be empty", HTTPD_400_BAD_REQUEST);
        }
        if (settings_set_file_prefix(item->valuestring) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Filename prefix cannot be larger than 70 characters", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "FILE_SEPARATOR_CHAR");
    if (item != NULL)
    {
        if (settings_set_file_separator(item->valueint) != ESP_OK)
        {
            json_send_resp(req, ENDPOINT_RESP_NACK, "Invalid file separator char value. Only 0=comma and 1=semicolon possible.", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

    // split size unit must be called beofer set_file_split_size
    item = cJSON_GetObjectItemCaseSensitive(settings_in, "FILE_SPLIT_SIZE_UNIT");
    if (item != NULL)
    {
        if (settings_set_file_split_size_unit(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Invalid file split size unit value.", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "FILE_SPLIT_SIZE");
    if (item != NULL)
    {
        if (settings_set_file_split_size(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Invalid file split size.  Min. 200 KB and Maximum 2 GB", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }


    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_SSID");
    if (item != NULL)
    {
        if (settings_set_wifi_ssid(item->valuestring) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Error setting Wifi SSID", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }
    
    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_MODE");
    if (item != NULL)
    {
        if (item->valueint == 0)
        {
            // Wifi ap mode
            settings_set_wifi_mode(WIFI_MODE_AP);
        } 
        else if (item->valueint == 1)
        {
            // Wifi ap/station mode
            settings_set_wifi_mode(WIFI_MODE_APSTA);
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_CHANNEL");
    if (item != NULL)
    {
        if (settings_set_wifi_channel(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Error setting Wifi channel", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_PASSWORD");
    if (item != NULL)
    {
        if (settings_set_wifi_password(item->valuestring) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Error setting Wifi SSID", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }


    item = cJSON_GetObjectItemCaseSensitive(settings_in, "ADC_RESOLUTION");
    if (item != NULL)
    {
        if (settings_set_resolution(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "ADC resolution missing or wrong value", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "LOG_SAMPLE_RATE");
    if (item != NULL)
    {
        if (settings_set_samplerate(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Log sample rate missing or wrong value", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "LOG_MODE");
    if (item != NULL)
    {
        if (settings_set_logmode(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Log mode missing or wrong value", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }


    item = cJSON_GetObjectItemCaseSensitive(settings_in, "TIMESTAMP");
    
    if (item != NULL)
    {
        if (settings_set_timestamp((uint64_t)item->valuedouble) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Timestamp missing or wrong value", HTTPD_400_BAD_REQUEST);
            // return ESP_FAIL;
            goto error;
        }
    }

 

    if (Logtask_sync_settings() == ESP_FAIL)
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Error storing settings", HTTPD_400_BAD_REQUEST);
        // return ESP_FAIL;
        goto error;
    }


    // First check if old wifi mode != new mode
    if (oldSettings.wifi_mode != settings_get_wifi_mode())
    {
        if (settings_get_wifi_mode() == WIFI_MODE_APSTA)
        {
            // wifi_connect_to_ap(); 
            // Send task to wifi queue to connect to access point
            // 
            // Logtask_wifi_connect_ap();
            wifi_connect_to_ap();
        } else {
            // send event to wifi to disconnect from access point
            // Logtask_wifi_disconnect_ap();
            wifi_disconnect_ap();
        }
    } else if // if any wifi setting has changed, then we need to reconnect to the access point when mode is WIFI_MODE_APSTA
        (settings_get_wifi_mode() == WIFI_MODE_APSTA && 
        (
        (wifi_is_connected_to_ap() == false)  ||
        (oldSettings.wifi_channel != settings_get_wifi_channel()) || 
        (strcmp(oldSettings.wifi_ssid_ap, settings_get_wifi_ssid_ap()) != 0)  || 
        (strcmp(oldSettings.wifi_password, settings_get_wifi_password()) !=0 )
        ))
    {
        // a connect event automatically disconnects and reconnects to an access point
        // Logtask_wifi_connect_ap();
        wifi_connect_to_ap();
    }

    // only send ack in case wifi mode has not changed. Else the next will get stuck
    json_send_resp(req, ENDPOINT_RESP_ACK, NULL, 0);
    

error:
    free((void*)settings_in);
error2:
    cJSON_Delete(root);
    

    return ESP_OK;
   
}

esp_err_t reboot_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

   if (Logging_restartSystem() != ESP_OK)
   {
    json_send_resp(req, ENDPOINT_RESP_NACK, "Cannot reboot system", HTTPD_500_INTERNAL_SERVER_ERROR);
    return ESP_FAIL;
   }

    json_send_resp(req, ENDPOINT_RESP_ACK, "Rebooting in 3 seconds. Logging stopping if not done already.", 0); 
    json_send_resp(req, ENDPOINT_RESP_ACK, NULL, 0);
    
    if (Logger_getState() == LOGTASK_LOGGING)
    {
        LogTask_stop();
    }

    ESP_LOGI("REST: ", "Going to reboot") ;

    

    return ESP_OK;
}

esp_err_t upload_form_handler(httpd_req_t *req)
{
// Send the HTML form as the response
httpd_resp_set_type(req, "text/html");
httpd_resp_send(req, UPLOAD_FORM, HTTPD_RESP_USE_STRLEN);
return ESP_OK;
}

static esp_err_t Logger_start_handler(httpd_req_t *req)
{
    
    if (LogTask_start() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK, NULL, HTTPD_403_FORBIDDEN);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Logger already logging", HTTPD_403_FORBIDDEN);
    }

    
    return ESP_OK;
}

static esp_err_t Logger_stop_handler(httpd_req_t *req)
{
    if (LogTask_stop() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK, NULL, 0);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Logger not logging", HTTPD_403_FORBIDDEN);
    }
    
    return ESP_OK;
}

esp_err_t  Logger_sdcard_unmount_handler(httpd_req_t *req)
{
    if(Logger_user_unmount_sdcard() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK, NULL, 0);
    }   else {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Logger logging or sd card not inserted", HTTPD_403_FORBIDDEN);
    }

    return ESP_OK;
}


// static esp_err_t logger_wifi_status_handler(httpd_req_t *req)
// {
//     httpd_resp_set_type(req, "application/json");
//     cJSON *root = cJSON_CreateObject();

//     cJSON_AddBoolToObject(root, (const char*)"connected", wifi_is_connected());
//     cJSON_AddStringToObject(root, (const char*)"ssid", wifi_get_ssid());
//     cJSON_AddStringToObject(root, (const char*)"ip", "dummy"());

//     const char *sys_info = cJSON_Print(root);
//     httpd_resp_sendstr(req, sys_info);
//     free((void*)sys_info);
//     cJSON_Delete(root);
//     return ESP_OK;
// }

esp_err_t json_send_resp(httpd_req_t *req, endpoint_response_t type, char * reason, httpd_err_code_t status_code)
{


    if (type == ENDPOINT_RESP_NACK)
    {
        httpd_resp_send_err(req, status_code, reason);
    } else {
        httpd_resp_set_type(req, "application/json");
        cJSON *root = cJSON_CreateObject();

        char str[6];

        switch (type)
        {
            case ENDPOINT_RESP_ACK:
                strcpy(str, "ack");
            break;

            case ENDPOINT_RESP_NACK:
                strcpy(str, "nack");
            break;

            
            case ENDPOINT_RESP_ERROR:
            default:
                strcpy(str, "error");
            break;

        }
        
        cJSON_AddStringToObject(root, (const char*)"resp", str);
        
        if (type == ENDPOINT_RESP_NACK && reason != NULL)
        {
            cJSON_AddStringToObject(root, (const char*)"reason", reason);
        }
        const char *sys_info = cJSON_Print(root);
        httpd_resp_sendstr(req, sys_info);
        free((void *)sys_info);
        cJSON_Delete(root);
    }

    return ESP_OK;
}


esp_err_t start_rest_server(const char *base_path)
{
    if (server != NULL)
    {
        ESP_LOGE(REST_TAG, "Server already started");
        return ESP_FAIL;
    }

    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    

    server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 18;
    config.task_priority = tskIDLE_PRIORITY+1;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    httpd_uri_t logger_calibrate_uri = {
        .uri = "/ajax/calibrate",
        .method = HTTP_POST,
        .handler = logger_calibrate_handler,
        .user_ctx = rest_context
    };

     httpd_register_uri_handler(server, &logger_calibrate_uri);

    httpd_uri_t logger_formatSdcard_uri = {
        .uri = "/ajax/formatSdcard",
        .method = HTTP_GET,
        .handler = logger_formatSdcard_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_formatSdcard_uri);


    httpd_uri_t logger_getDefaultConfig_uri = {
        .uri = "/ajax/getDefaultConfig",
        .method = HTTP_GET,
        .handler = logger_getDefaultConfig,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getDefaultConfig_uri);

    httpd_uri_t logger_getConfig_uri = {
        .uri = "/ajax/getConfig",
        .method = HTTP_GET,
        .handler = logger_getConfig_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getConfig_uri);

    httpd_uri_t logger_getValues_uri = {
        .uri = "/ajax/getValues",
        .method = HTTP_GET,
        .handler = logger_getValues_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getValues_uri);

    httpd_uri_t logger_getRawAdc_uri = {
        .uri = "/ajax/getRawAdc",
        .method = HTTP_GET,
        .handler = logger_getRawAdc_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getRawAdc_uri);

    

    httpd_uri_t logger_getStatus_uri = {
        .uri = "/ajax/getStatus",
        .method = HTTP_GET,
        .handler = logger_getStatus_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getStatus_uri);

    httpd_uri_t logger_setTime_uri = {
        .uri = "/ajax/setTime",
        .method = HTTP_POST,
        .handler = logger_setTime_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_setTime_uri);

    httpd_uri_t logger_setConfig_uri = {
        .uri = "/ajax/setConfig",
        .method = HTTP_POST,
        .handler = logger_setConfig_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_setConfig_uri);

    httpd_uri_t logger_stop_uri = {
        .uri = "/ajax/loggerStop",
        .method = HTTP_POST,
        .handler = Logger_stop_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &logger_stop_uri);

    httpd_uri_t logger_start_uri = {
        .uri = "/ajax/loggerStart",
        .method = HTTP_POST,
        .handler = Logger_start_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &logger_start_uri);

    // Create endpoint for sdcard unmount
    httpd_uri_t sdcard_unmount_uri = {
        .uri = "/ajax/sdcardUnmount",
        .method = HTTP_POST,
        .handler = Logger_sdcard_unmount_handler,
        .user_ctx = rest_context
    };

     httpd_register_uri_handler(server, &sdcard_unmount_uri);

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri       = "/ajax/getFileList/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = download_get_handler,
        .user_ctx  = rest_context    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);

    httpd_uri_t file_fwupdate = {
        .uri       = "/fwupdate/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = fwupdate_get_handler,
        .user_ctx  = rest_context    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_fwupdate);

    
    httpd_uri_t reboot = {
        .uri       = "/ajax/reboot",  // Match all URIs of type /path/to/file
        .method    = HTTP_POST,
        .handler   = reboot_post_handler,
        .user_ctx  = rest_context    // Pass server data as context
    };
    httpd_register_uri_handler(server, &reboot);

    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = rest_context    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = rest_context    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);

        /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);


    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}


esp_err_t stop_rest_server(void)
{
    httpd_handle_t server = NULL;
    
    if (server == NULL)
    {
        ESP_LOGE(REST_TAG, "Server not started");
        return ESP_FAIL;
    }
    httpd_stop(server);
    
    server = NULL;
    return ESP_OK;
}