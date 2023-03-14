/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>

#include "rest_server.h"
#include "config.h"

char * endpoint_response_char[] = 
{
    "OK",
    "NACK",
    "ERROR"
};



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

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    } else if (CHECK_FILE_EXTENSION(filepath, ".gz")) {
        type = "application/javscript";
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
    
    if (strncmp(filepath, rest_context->base_path, strlen(filepath) - 1) == 0)
    {
        strlcat(filepath, "index.html", sizeof(filepath));
    }

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        if (CHECK_FILE_EXTENSION(filepath, ".js"))
        {
            strlcat(filepath, ".gz", sizeof(filepath));
        }
        ESP_LOGE(REST_TAG, "Trying %s", filepath);
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

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
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
    converted_reading_t data;
    
    
    Logger_GetSingleConversion(&data);
    
    cJSON_AddNumberToObject(root, "TIMESTAMP", data.timestamp);
    
    cJSON *readings = cJSON_AddObjectToObject(root, "READINGS");

    cJSON *temperature = cJSON_AddObjectToObject(readings, "TEMPERATURE");
    cJSON_AddStringToObject(temperature, "UNITS", "DEG C");
    cJSON *tValues = cJSON_AddObjectToObject(temperature, "VALUES");

    cJSON *analog = cJSON_AddObjectToObject(readings, "ANALOG");
    cJSON_AddStringToObject(analog, "UNITS", "Volt");
    cJSON * aValues = cJSON_AddObjectToObject(analog, "VALUES");
    
    char buf[5];
    for (int i=ADC_CHANNEL_0; i<=ADC_CHANNEL_7; i++)
    {
        if (settings_get_adc_channel_type(i))
        {
            sprintf(buf,"T%d", i);
             cJSON_AddNumberToObject(tValues, buf, data.temperatureData[i]);
        } else {
            sprintf(buf,"AIN%d", i);
            cJSON_AddNumberToObject(aValues, buf, data.analogData[i]);
        }
        
    }
   
    // cJSON_AddNumberToObject(tValues, "T1", data.temperatureData[1]);
    // cJSON_AddNumberToObject(tValues, "T2", data.temperatureData[2]);
    // cJSON_AddNumberToObject(tValues, "T3", data.temperatureData[3]);
    // cJSON_AddNumberToObject(tValues, "T4", data.temperatureData[4]);
    // cJSON_AddNumberToObject(tValues, "T5", data.temperatureData[5]);
    // cJSON_AddNumberToObject(tValues, "T6", data.temperatureData[6]);
    // cJSON_AddNumberToObject(tValues, "T7", data.temperatureData[7]);

    
    
   
    // // For future use. Always enabled now.
    // // cJSON_AddNumberToObject(root, "adc_channels_enabled", settings->adc_channels_enabled);

    // cJSON_AddNumberToObject(aValues, "AIN0", data.analogData[0]);
    // cJSON_AddNumberToObject(aValues, "AIN1", data.analogData[1]);
    // cJSON_AddNumberToObject(aValues, "AIN2", data.analogData[2]);
    // cJSON_AddNumberToObject(aValues, "AIN3", data.analogData[3]);
    // cJSON_AddNumberToObject(aValues, "AIN4", data.analogData[4]);
    // cJSON_AddNumberToObject(aValues, "AIN5", data.analogData[5]);
    // cJSON_AddNumberToObject(aValues, "AIN6", data.analogData[6]);
    // cJSON_AddNumberToObject(aValues, "AIN7", data.analogData[7]);
    
   cJSON *digital = cJSON_AddObjectToObject(readings, "DIGITAL");
   cJSON_AddStringToObject(digital, "UNITS", "Level");
//    cJSON *dValues = cJSON_AddObjectToObject(readings, "VALUES");
    
    cJSON_AddNumberToObject(digital, "DI0", data.gpioData[0]);
    cJSON_AddNumberToObject(digital, "DI1", data.gpioData[1]);
    cJSON_AddNumberToObject(digital, "DI2", data.gpioData[2]);
    cJSON_AddNumberToObject(digital, "DI3", data.gpioData[3]);
    cJSON_AddNumberToObject(digital, "DI4", data.gpioData[4]);
    cJSON_AddNumberToObject(digital, "DI5", data.gpioData[5]);



    
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
    cJSON_AddNumberToObject(root, "T_CHIP", sysinfo_get_core_temperature());
    cJSON_AddStringToObject(root, "FW_VERSION", sysinfo_get_fw_version());
    cJSON_AddNumberToObject(root, "SD_CARD_FREE_SPACE", esp_sd_card_get_free_space());
    cJSON_AddNumberToObject(root, "SD_CARD_STATUS", esp_sd_card_get_state());

    const char *settings_json= cJSON_Print(root);
    httpd_resp_sendstr(req, settings_json);
     free((void *)settings_json);
    cJSON_Delete(root);
    return ESP_OK;
}


static esp_err_t logger_getConfig_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON * root = cJSON_CreateObject();
    if (root == NULL)
    {

        return ESP_FAIL;
    }
  
    
    Settings_t *settings = settings_get();

    cJSON *range_select = cJSON_AddObjectToObject(root, "AIN_RANGE_SELECT");
    cJSON *ntc_select = cJSON_AddObjectToObject(root, "NTC_SELECT");
    char buf[5];

    for (int i=ADC_CHANNEL_0; i<=ADC_CHANNEL_7; i++)
    {

            sprintf(buf,"NTC%d", i);
            cJSON_AddBoolToObject(ntc_select, buf, settings_get_adc_channel_type(i));

            sprintf(buf,"AIN%d", i);
             cJSON_AddBoolToObject(range_select, buf, settings_get_adc_channel_range(i));

    }
    
    cJSON_AddStringToObject(root, "WIFI_SSID", settings->wifi_ssid);
    cJSON_AddNumberToObject(root, "WIFI_CHANNEL", settings->wifi_channel);
    cJSON_AddStringToObject(root, "WIFI_PASSWORD", settings->wifi_password);

    cJSON_AddNumberToObject(root, "ADC_RESOLUTION", settings->adc_resolution);
    cJSON_AddNumberToObject(root, "LOG_SAMPLE_RATE", settings->log_sample_rate);
    cJSON_AddNumberToObject(root, "LOG_MODE", settings->logMode);


    
    const char *settings_json= cJSON_Print(root);
    httpd_resp_sendstr(req, settings_json);
     free((void *)settings_json);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t logger_setConfig_handler(httpd_req_t *req)
{

    httpd_resp_set_type(req, "application/json");
    cJSON *settings_in = NULL;
    if (Logger_getState() == LOGTASK_LOGGING)
    {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Cannot set settings while logging");
        
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

    
    char buf2[30];
    for (int j = 0; j<2; j++)
    {
        for (int i = 0; i<8; i++)
        {
            if (j ==0)
            {
                item = cJSON_GetObjectItemCaseSensitive(settings_in, "NTC_SELECT");
                if (item != NULL)
                {
                    sprintf(buf2, "NTC%d", i);
                    // json_send_resp(req, ENDPOINT_RESP_ERROR);
                    // return ESP_FAIL;
                }

                
            } else {
                item = cJSON_GetObjectItemCaseSensitive(settings_in, "AIN_RANGE_SELECT");
                if (item != NULL)
                {
                    sprintf(buf2, "AIN%d", i);
                    // json_send_resp(req, ENDPOINT_RESP_ERROR);
                    // return ESP_FAIL;
                }
                
            }
                
            cJSON * subItem = cJSON_GetObjectItemCaseSensitive(item, buf2);
            if (subItem != NULL)
            {
                if (j==0)
                {
                    #ifdef DEBUG_REST_SERVER
                    ESP_LOGI("REST: ", "NTC%d %d", i, (subItem->valueint));
                    #endif
                    settings_set_adc_channel_type(i, subItem->valueint);
                } else {
                    #ifdef DEBUG_REST_SERVER
                    ESP_LOGI("REST: ", "AIN%d %d", i, (subItem->valueint));
                    #endif
                    settings_set_adc_channel_range(i, subItem->valueint);
                }
                
            } 
            // else {
            //     json_send_resp(req, ENDPOINT_RESP_ERROR);
            //     return ESP_FAIL;
            // }
        
        }
    }
        
    // item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_SSID");
    // if (item == NULL || settings_set_wifi_ssid(item->valuestring))
    // {
    //     ESP_LOGE("REST: ", "Error setting Wifi SSID");
    //     json_send_resp(req, ENDPOINT_RESP_ERROR);
    //     return ESP_FAIL;
    // }

    // item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_CHANNEL");
    // if (item == NULL || settings_set_wifi_channel(item->valueint))
    // {
    //     ESP_LOGE("REST: ", "Error setting Wifi channel");
    //     json_send_resp(req, ENDPOINT_RESP_ERROR);
    //     return ESP_FAIL;
    // }

    // item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_PASSWORD");
    // if (item == NULL || settings_set_wifi_password(item->valuestring))
    // {
    //     ESP_LOGE("REST: ", "Error setting Wifi SSID");
    //     json_send_resp(req, ENDPOINT_RESP_ERROR);
    //     return ESP_FAIL;
    // }


    // item = cJSON_GetObjectItemCaseSensitive(settings_in, "ADC_RESOLUTION");
    // if (item == NULL || settings_set_resolution(item->valueint))
    // {
    //     ESP_LOGE("REST: ", "ADC resolution missing or wrong value");
    //     json_send_resp(req, ENDPOINT_RESP_ERROR);
    //     return ESP_FAIL;
    // }

    // item = cJSON_GetObjectItemCaseSensitive(settings_in, "LOG_SAMPLE_RATE");
    // if (item == NULL || settings_set_samplerate(item->valueint))
    // {
    //     ESP_LOGE("REST: ", "Log sample rate missing or wrong value");
    //     json_send_resp(req, ENDPOINT_RESP_ERROR);
    //     return ESP_FAIL;
    // }

    // item = cJSON_GetObjectItemCaseSensitive(settings_in, "LOG_MODE");
    // if (item == NULL || settings_set_logmode(item->valueint))
    // {
    //     ESP_LOGE("REST: ", "Log mode missing or wrong value");
    //     json_send_resp(req, ENDPOINT_RESP_ERROR);
    //     return ESP_FAIL;
    // }

    // item = cJSON_GetObjectItemCaseSensitive(settings_in, "TIMESTAMP");
    // if (item == NULL || settings_set_timestamp(item->valueint))
    // {
    //     ESP_LOGE("REST: ", "Log mode missing or wrong value");
    //     json_send_resp(req, ENDPOINT_RESP_ERROR);
    //     return ESP_FAIL;
    // }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_SSID");
    if (item != NULL)
    {
        if (settings_set_wifi_ssid(item->valuestring) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Error setting Wifi SSID");
            // return ESP_FAIL;
            goto error;
        }
    }
    

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_CHANNEL");
    if (item != NULL)
    {
        if (settings_set_wifi_channel(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Error setting Wifi channel");
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "WIFI_PASSWORD");
    if (item != NULL)
    {
        if (settings_set_wifi_password(item->valuestring) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Error setting Wifi SSID");
            // return ESP_FAIL;
            goto error;
        }
    }


    item = cJSON_GetObjectItemCaseSensitive(settings_in, "ADC_RESOLUTION");
    if (item != NULL)
    {
        if (settings_set_resolution(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "ADC resolution missing or wrong value");
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "LOG_SAMPLE_RATE");
    if (item != NULL)
    {
        if (settings_set_samplerate(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Log sample rate missing or wrong value");
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "LOG_MODE");
    if (item != NULL)
    {
        if (settings_set_logmode(item->valueint) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Log mode missing or wrong value");
            // return ESP_FAIL;
            goto error;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(settings_in, "TIMESTAMP");
    
    if (item != NULL)
    {
        if (settings_set_timestamp((uint64_t)item->valuedouble) != ESP_OK)
        {
            
            json_send_resp(req, ENDPOINT_RESP_NACK, "Timestamp missing or wrong value");
            // return ESP_FAIL;
            goto error;
        }
    }

    Logger_syncSettings();
    json_send_resp(req, ENDPOINT_RESP_ACK, NULL);
    

error:
    free((void*)settings_in);
error2:
    cJSON_Delete(root);
    

    return ESP_OK;
   
}



static esp_err_t Logger_start_handler(httpd_req_t *req)
{
    
    if (LogTask_start() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK, NULL);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Logger already logging");
    }

    
    return ESP_OK;
}

static esp_err_t Logger_stop_handler(httpd_req_t *req)
{
    if (LogTask_stop() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK, NULL);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK, "Logger not logging");
    }
    
    return ESP_OK;
}

esp_err_t json_send_resp(httpd_req_t *req, endpoint_response_t type, char * reason)
{

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

    return ESP_OK;
}


esp_err_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = tskIDLE_PRIORITY+1;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);




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

    httpd_uri_t logger_getStatus_uri = {
        .uri = "/ajax/getStatus",
        .method = HTTP_GET,
        .handler = logger_getStatus_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getStatus_uri);

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
