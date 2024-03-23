#ifndef _REST_SERVER_H
#define _REST_SERVER_H
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "logger.h"
#include "common.h"
#include "esp_sd_card.h"

#include "sysinfo.h"

typedef enum
{
    ENDPOINT_RESP_ACK = 0,
    ENDPOINT_RESP_NACK = 1,
    ENDPOINT_RESP_ERROR = 2
} endpoint_response_t;

const char * logger_settings_to_json(Settings_t *settings);
esp_err_t json_send_resp(httpd_req_t *req, endpoint_response_t type, char * reason, httpd_err_code_t status_code);
esp_err_t start_rest_server(const char *base_path);
esp_err_t stop_rest_server(void);

#endif