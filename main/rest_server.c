/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>

#include "rest_server.h"

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
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

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
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
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
    cJSON_AddNumberToObject(root, "CPU_TEMP", sysinfo_get_core_temperature());
    cJSON_AddStringToObject(root, "FirmwareVersion", sysinfo_get_fw_version());
    cJSON * sdcard = cJSON_AddObjectToObject(root, "sd_card");

    cJSON_AddNumberToObject(sdcard, "free_space_kib", esp_sd_card_get_free_space());
    cJSON_AddNumberToObject(sdcard, "state", esp_sd_card_get_state());;

    const char *settings_json= cJSON_Print(root);
    httpd_resp_sendstr(req, settings_json);
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

    // int total_len = req->content_len;
    // int cur_len = 0;
    // char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    // int received = 0;
    // if (total_len >= SCRATCH_BUFSIZE) {
    //     /* Respond with 500 Internal Server Error */
    //     httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
    //     return ESP_FAIL;
    // }
    // while (cur_len < total_len) {
    //     received = httpd_req_recv(req, buf + cur_len, total_len);
    //     if (received <= 0) {
    //         /* Respond with 500 Internal Server Error */
    //         httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
    //         return ESP_FAIL;
    //     }
    //     cur_len += received;
    // }
    // buf[total_len] = '\0';
    Settings_t *settings = settings_get();

    cJSON *range_select = cJSON_AddObjectToObject(root, "AIN_RANGE_SELECT");
    cJSON *ntc_select = cJSON_AddObjectToObject(root, "NTC_SELECT");

    cJSON_AddBoolToObject(ntc_select, "NTC0", settings_get_adc_channel_type(ADC_CHANNEL_0));
    cJSON_AddBoolToObject(ntc_select, "NTC1", settings_get_adc_channel_type(ADC_CHANNEL_1));
    cJSON_AddBoolToObject(ntc_select, "NTC2", settings_get_adc_channel_type(ADC_CHANNEL_2));
    cJSON_AddBoolToObject(ntc_select, "NTC3", settings_get_adc_channel_type(ADC_CHANNEL_3));
    cJSON_AddBoolToObject(ntc_select, "NTC4", settings_get_adc_channel_type(ADC_CHANNEL_4));
    cJSON_AddBoolToObject(ntc_select, "NTC5", settings_get_adc_channel_type(ADC_CHANNEL_5));
    cJSON_AddBoolToObject(ntc_select, "NTC6", settings_get_adc_channel_type(ADC_CHANNEL_6));
    cJSON_AddBoolToObject(ntc_select, "NTC7", settings_get_adc_channel_type(ADC_CHANNEL_7));

    

    // For future use. Always enabled now.
    // cJSON_AddNumberToObject(root, "adc_channels_enabled", settings->adc_channels_enabled);

    cJSON_AddBoolToObject(range_select, "AIN0", settings_get_adc_channel_range(ADC_CHANNEL_0));
    cJSON_AddBoolToObject(range_select, "AIN1", settings_get_adc_channel_range(ADC_CHANNEL_1));
    cJSON_AddBoolToObject(range_select, "AIN2", settings_get_adc_channel_range(ADC_CHANNEL_2));
    cJSON_AddBoolToObject(range_select, "AIN3", settings_get_adc_channel_range(ADC_CHANNEL_3));
    cJSON_AddBoolToObject(range_select, "AIN4", settings_get_adc_channel_range(ADC_CHANNEL_4));
    cJSON_AddBoolToObject(range_select, "AIN5", settings_get_adc_channel_range(ADC_CHANNEL_5));
    cJSON_AddBoolToObject(range_select, "AIN6", settings_get_adc_channel_range(ADC_CHANNEL_6));
    cJSON_AddBoolToObject(range_select, "AIN7", settings_get_adc_channel_range(ADC_CHANNEL_7));
    
    cJSON_AddNumberToObject(root, "adc_resolution", settings->adc_resolution);
    cJSON_AddNumberToObject(root, "log_sample_rate", settings->log_sample_rate);
    cJSON_AddNumberToObject(root, "log_mode", settings->logMode);
    
    const char *settings_json= cJSON_Print(root);
    httpd_resp_sendstr(req, settings_json);
     free((void *)settings_json);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t Logger_start_handler(httpd_req_t *req)
{
    
    if (Logger_start() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK);
    }

    
    return ESP_OK;
}

static esp_err_t Logger_stop_handler(httpd_req_t *req)
{
    if (Logger_stop() == ESP_OK)
    {
        json_send_resp(req, ENDPOINT_RESP_ACK);
    } else {
        json_send_resp(req, ENDPOINT_RESP_NACK);
    }
    
    return ESP_OK;
}

esp_err_t json_send_resp(httpd_req_t *req, endpoint_response_t type)
{

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    Logger_stop();
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
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    // /* URI handler for fetching system info */
    // httpd_uri_t system_info_get_uri = {
    //     .uri = "/api/v1/system/info",
    //     .method = HTTP_GET,
    //     .handler = system_info_get_handler,
    //     .user_ctx = rest_context
    // };
    // httpd_register_uri_handler(server, &system_info_get_uri);

    // /* URI handler for fetching temperature data */
    // httpd_uri_t temperature_data_get_uri = {
    //     .uri = "/api/v1/temp/raw",
    //     .method = HTTP_GET,
    //     .handler = temperature_data_get_handler,
    //     .user_ctx = rest_context
    // };
    // httpd_register_uri_handler(server, &temperature_data_get_uri);

    // /* URI handler for light brightness control */
    // httpd_uri_t logger_start_uri = {
    //     .uri = "/api/v1/logger/start",
    //     .method = HTTP_POST,
    //     .handler = Logger_start_handler,
    //     .user_ctx = rest_context
    // };
    // httpd_register_uri_handler(server, &logger_start_uri);

    httpd_uri_t logger_getStatus_uri = {
        .uri = "/ajax/getState",
        .method = HTTP_GET,
        .handler = logger_getStatus_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getStatus_uri);

    httpd_uri_t logger_getConfig_uri = {
        .uri = "/ajax/getConfig",
        .method = HTTP_GET,
        .handler = logger_getConfig_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &logger_getConfig_uri);

    httpd_uri_t logger_stop_uri = {
        .uri = "/ajax/loggerStop",
        .method = HTTP_GET,
        .handler = Logger_stop_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &logger_stop_uri);

    httpd_uri_t logger_start_uri = {
        .uri = "/ajax/loggerStart",
        .method = HTTP_GET,
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
