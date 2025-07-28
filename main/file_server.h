/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#ifndef __FILE_SERVER_H__
#define __FILE_SERVER_H__
#include "esp_system.h"
#include "esp_http_server.h"

 esp_err_t download_get_handler(httpd_req_t *req);
 esp_err_t upload_post_handler(httpd_req_t *req);
 esp_err_t delete_post_handler(httpd_req_t *req);
 esp_err_t fwupdate_get_handler(httpd_req_t *req);

#endif