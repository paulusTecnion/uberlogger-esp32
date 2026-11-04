/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "file_server.h"
#include "rest_server.h"
#include "esp_sd_card.h"
#include "firmware-www.h"
#include "firmwareESP32.h"
#include "firmwareSTM32.h"
#include "logger.h"


/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (1200*1024) // 1MB
#define MAX_FILE_SIZE_STR "1MB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

extern SemaphoreHandle_t idle_state;
static const char *TAG_FILESERVER = "file_server";

/* Drain unread request body so the HTTP error response reaches the client
 * before the connection closes.  Without this, a TCP RST can race the
 * error response and the browser sees a network error instead of a 4xx/5xx. */
static void upload_drain_request(httpd_req_t *req)
{
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int remaining = req->content_len;
    while (remaining > 0) {
        int n = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE));
        if (n <= 0) break;
        remaining -= n;
    }
}

// Helper function to get query parameter value from a request
static char* get_query_param_value(httpd_req_t *req, const char *param_name) {
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1) {
        return NULL;
    }

    char* buf = malloc(buf_len);
    if (!buf) {
        return NULL;
    }

    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        char* param_value = NULL;
        if (httpd_query_key_value(buf, param_name, buf, buf_len) == ESP_OK) {
            param_value = strdup(buf);
        }
        free(buf);
        return param_value;
    } else {
        free(buf);
        return NULL;
    }
}


/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    // Default values for pagination
    int page = 999;// default page number. 999 = last page
    int limit = 25; // maximum number of files per page
    int skip = 0; // counter for files to be skipped
    int processed = 0; // counter for files that are processed within a page

    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "Cannot mount SD card");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot mount SD card");
        return ESP_FAIL;
    }

    const char* percentTwenty = "%20";
    const char space = ' ';
    char* found;

    // Replace %20 in dirpath with space
    while ((found = strstr(dirpath, percentTwenty)) != NULL) {
        memcpy(found, &space, sizeof(char)); // Replace % with space
        memmove(found + 1, found + 3, strlen(found + 3) + 1); // Shift the rest of the string
    }

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE(TAG_FILESERVER, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

     // Count all files/directories
    int total_entries = 0;
    // struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        total_entries++;
    }
    closedir(dir);  // Close the directory after counting
    dir = opendir(dirpath);
    // Calculate total pages
    int total_pages = (total_entries + limit - 1) / limit;  // This ensures rounding up
   
    /* Iterate over all files / folders and fetch their names and sizes */
    int i = 1;

    httpd_resp_set_type(req, "application/json");
    
    // create new root cjson
    cJSON * main = cJSON_CreateObject();
    if (main == NULL)
    {
        return ESP_FAIL;
    }

    cJSON * root = cJSON_AddObjectToObject(main, "root");
    char int2str[11];

    // Parse page and limit from request
    char *page_param = get_query_param_value(req, "filepage");
    // char *limit_param = get_query_param_value(req, "limit");
    if (page_param) {
        page = atoi(page_param);
        free(page_param);
    }
    
    if (page > total_pages) page = total_pages; // By default go to last page in order to see latest files. 
    // if (limit_param) {
    //     limit = atoi(limit_param);
    //     free(limit_param);
    // }
    if (page < 1) page = 1;
    // if (limit < 1) limit = 10;
    skip = (page - 1) * limit;

    while ((entry = readdir(dir)) != NULL ) {
        
            if (processed >= limit) break; // Stop if the limit is reached
            if (skip > 0) {
                skip--;  // Skip this entry, decrease the skip count
                continue; // Go to the next iteration of the loop
            }
            entrytype = (entry->d_type == DT_DIR ? "DIRECTORY" : "FILE");
            // Filter out directory with .Trash in it
            if (entry->d_type == DT_DIR && strstr(entry->d_name, ".Trash") != NULL)
            {
                continue;
            }

            strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
            if (stat(entrypath, &entry_stat) == -1) {
                ESP_LOGE(TAG_FILESERVER, "Failed to stat %s : %s", entrytype, entry->d_name);
                continue;
            }
            sprintf(entrysize, "%ld", entry_stat.st_size);
            #ifdef DEBUG_FILESERVER
            ESP_LOGI(TAG_FILESERVER, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
            #endif
            /* Send chunk of HTML file containing table entries with file name and size */

            // Add entry to cJSON
            sprintf(int2str, "%d", i);
            cJSON * item = cJSON_AddObjectToObject(root, int2str);
            cJSON_AddStringToObject(item, "NAME", entry->d_name);
            cJSON_AddStringToObject(item, "TYPE", entrytype);
            cJSON_AddStringToObject(item, "SIZE", entrysize);
            i++;
            processed++;
    }
    closedir(dir);

     // Create and add pagination info to JSON
    cJSON *pagination_info = cJSON_AddObjectToObject(main, "pagination");
    cJSON_AddNumberToObject(pagination_info, "current_page", page);
    cJSON_AddNumberToObject(pagination_info, "total_pages", total_pages);
    cJSON_AddNumberToObject(pagination_info, "total_entries", total_entries);
    cJSON_AddNumberToObject(pagination_info, "entries_per_page", limit);

    char* json_resp = cJSON_Print(main);
    #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILESERVER, "%s", json_resp);
    #endif
    // Send cJSON
    httpd_resp_send(req, json_resp, strlen(json_resp));
    free(json_resp);
    cJSON_Delete(main);

    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".csv")) {
        return httpd_resp_set_type(req, "text/csv");
    } 
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
/* Decode %XX escapes in place (the web UI URL-encodes file names). */
static void uri_decode_inplace(char *s)
{
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hex[3] = { p[1], p[2], 0 };
            *o++ = (char)strtol(hex, NULL, 16);
            p += 2;
        } else {
            *o++ = *p;
        }
    }
    *o = 0;
}

static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);
    uri_decode_inplace(dest + base_pathlen);

    /* Refuse paths that try to leave the base directory */
    const char *p = dest + base_pathlen;
    if (strstr(p, "/../") || (strlen(p) >= 3 && strcmp(p + strlen(p) - 3, "/..") == 0)) {
        return NULL;
    }

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* ---- Double-buffered file download ----
 * A reader task fills one buffer from the SD card while the httpd task sends
 * the other over Wi-Fi, so SD and network latency overlap instead of adding.
 * Buffers circulate between two queues: free_q (empty, owned by reader) and
 * full_q (filled, owned by sender). A chunk with len == 0 marks EOF, len < 0
 * a read error; after posting it the reader never touches the context again. */

#define DL_READER_STACK_SIZE 4096
#define DL_NUM_BUFFERS       2

typedef struct {
    char *buf;
    int len;
} dl_chunk_t;

typedef struct {
    int fd;
    QueueHandle_t free_q;
    QueueHandle_t full_q;
    TaskHandle_t parent;
    volatile bool abort;
} dl_ctx_t;

static void dl_reader_task(void *arg)
{
    dl_ctx_t *ctx = (dl_ctx_t *)arg;
    /* Copy what we need after the final queue post: ctx lives on the
     * sender's stack and may be gone once the terminal chunk is received. */
    TaskHandle_t parent = ctx->parent;
    QueueHandle_t full_q = ctx->full_q;
    dl_chunk_t c;

    for (;;) {
        xQueueReceive(ctx->free_q, &c, portMAX_DELAY);
        if (ctx->abort) {
            c.len = 0;
            break;
        }
        /* read(), not fread(): stdio's 128-byte FATFS buffer makes fread
         * ~3x slower (measured 540 vs 1610 KB/s for 8 KB reads) */
        ssize_t n = read(ctx->fd, c.buf, SCRATCH_BUFSIZE);
        if (n <= 0) {
            c.len = (n < 0) ? -1 : 0;
            break;
        }
        c.len = (int)n;
        xQueueSend(full_q, &c, portMAX_DELAY);
    }

#ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "dl_reader stack high-water mark: %u bytes",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
#endif
    xQueueSend(full_q, &c, portMAX_DELAY);
    xTaskNotifyGive(parent);
    vTaskDelete(NULL);
}

/* Sequential fallback, used if the reader task or second buffer can't be allocated */
static esp_err_t send_file_sequential(httpd_req_t *req, int fd, char *chunk)
{
    ssize_t chunksize;
    while ((chunksize = read(fd, chunk, SCRATCH_BUFSIZE)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    return (chunksize < 0) ? ESP_FAIL : ESP_OK;
}

static esp_err_t send_file_double_buffered(httpd_req_t *req, int fd, char *scratch)
{
    char *second = heap_caps_malloc(SCRATCH_BUFSIZE, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    dl_ctx_t ctx = {
        .fd = fd,
        .free_q = xQueueCreate(DL_NUM_BUFFERS, sizeof(dl_chunk_t)),
        .full_q = xQueueCreate(DL_NUM_BUFFERS + 1, sizeof(dl_chunk_t)),
        .parent = xTaskGetCurrentTaskHandle(),
        .abort = false,
    };

#ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "Download start: free heap %u, largest block %u, min ever %u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#endif
    if (!second || !ctx.free_q || !ctx.full_q) {
        ESP_LOGW(TAG_FILESERVER, "Double buffer alloc failed, sending sequentially");
        goto fallback;
    }

    dl_chunk_t c = { .buf = scratch, .len = 0 };
    xQueueSend(ctx.free_q, &c, 0);
    c.buf = second;
    xQueueSend(ctx.free_q, &c, 0);

    /* Clear any stale notification before we rely on it */
    ulTaskNotifyTake(pdTRUE, 0);

    if (xTaskCreate(dl_reader_task, "dl_reader", DL_READER_STACK_SIZE, &ctx,
                    uxTaskPriorityGet(NULL), NULL) != pdPASS) {
        ESP_LOGW(TAG_FILESERVER, "Reader task create failed, sending sequentially");
        goto fallback;
    }

    esp_err_t ret = ESP_OK;
    for (;;) {
        xQueueReceive(ctx.full_q, &c, portMAX_DELAY);
        if (c.len <= 0) {
            if (c.len < 0) {
                ESP_LOGE(TAG_FILESERVER, "SD read error during download");
                ret = ESP_FAIL;
            }
            break;
        }
        if (ret == ESP_OK && httpd_resp_send_chunk(req, c.buf, c.len) != ESP_OK) {
            ret = ESP_FAIL;
            ctx.abort = true;   /* reader stops at its next buffer */
        }
        xQueueSend(ctx.free_q, &c, 0);
    }

    /* Wait until the reader has posted its last message and is about to exit */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    vQueueDelete(ctx.free_q);
    vQueueDelete(ctx.full_q);
    free(second);
    return ret;

fallback:
    if (ctx.free_q) vQueueDelete(ctx.free_q);
    if (ctx.full_q) vQueueDelete(ctx.full_q);
    free(second);
    return send_file_sequential(req, fd, scratch);
}

/* Handler to download a file kept on the server */
esp_err_t download_get_handler(httpd_req_t *req)
{
    if (!rest_check_auth(req)) {
        return rest_send_auth_required(req);
    }

      if (Logger_getState() == LOGTASK_LOGGING){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot request files while logging");
        return ESP_FAIL;
    }

    char filepath[FILE_PATH_MAX];
    int fd = -1;
    struct stat file_stat;

    // Strip out the /data part
    const char *data_substr = "/ajax/getFileList";
    const char *data_pos = strstr(req->uri, data_substr);
    
    // create new buffer
    char *new_uri = malloc(strlen(req->uri) - strlen(data_substr) + 1);
    if (new_uri == NULL) {
        ESP_LOGE(TAG_FILESERVER, "Failed to allocate memory for new_uri");
        return ESP_FAIL;
    }

    // copy the string up to the /data part
    strncpy(new_uri, req->uri, data_pos - req->uri);

    // copy the string after the /data part
    strncpy(new_uri + (data_pos - req->uri), data_pos + strlen(data_substr), strlen(req->uri) - strlen(data_substr) - (data_pos - req->uri) + 1);


    const char *filename = get_path_from_uri(filepath, "/sdcard",
                                             new_uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG_FILESERVER, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename too long");
        return ESP_FAIL;
    }

    // Replace %20 in filepath with space
    char *pos;
    while ((pos = strstr(filepath, "%%20")) != NULL)
    {
        *pos = ' ';
    }


    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
        return http_resp_dir_html(req, filepath);
    }

    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "Failed to mount SD card");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to mount SD card");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        ESP_LOGE(TAG_FILESERVER, "%s", filename);
        
        ESP_LOGE(TAG_FILESERVER, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }



    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG_FILESERVER, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }
    #ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    #endif
    set_content_type_from_file(req, filename);

    /* Stream the file: SD reads (reader task) overlap with Wi-Fi sends */
    char *scratch = ((struct file_server_data *)req->user_ctx)->scratch;
    esp_err_t send_ret = send_file_double_buffered(req, fd, scratch);

    /* Close file after sending complete (reader task has finished with it) */
    close(fd);

    if (send_ret != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "File sending failed!");
        /* Abort sending file */
        httpd_resp_sendstr_chunk(req, NULL);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
    }

    // esp_sd_card_unmount();
    #ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "File sending complete");
    #endif
    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
esp_err_t upload_post_handler(httpd_req_t *req)
{
    if (!rest_check_auth(req)) {
        return rest_send_auth_required(req);
    }
      if (Logger_getState() == LOGTASK_LOGGING ){
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot upload files while logging");
        return ESP_FAIL;
    }
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    // struct stat file_stat;


    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, "/sdcard",
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        upload_drain_request(req);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename too long");
        goto error;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG_FILESERVER, "Invalid filename : %s", filename);
        upload_drain_request(req);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        goto error;
    }

    // if (stat(filepath, &file_stat) == 0) {
    //     ESP_LOGE(TAG_FILESERVER, "File already exists : %s", filepath);
    //     /* Respond with 400 Bad Request */
    //     httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
    //     return ESP_FAIL;
    // }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG_FILESERVER, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        goto error;
    }

    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "Failed to mount SD card");
        upload_drain_request(req);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No SD card detected. Insert an SD card and try again.");
        goto error;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG_FILESERVER, "Failed to create file : %s", filepath);
        upload_drain_request(req);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file on SD card");
        goto error;
    }
    #ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "Receiving file : %s...", filename);
    #endif
    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        // ESP_LOGI(TAG_FILESERVER, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG_FILESERVER, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            goto error;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG_FILESERVER, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            goto error;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    // esp_sd_card_unmount();
    #ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "File reception complete");
    #endif
    // /* Redirect onto root to see the updated file list */
    // httpd_resp_set_status(req, "303 See Other");
    // httpd_resp_set_hdr(req, "Location", "/data/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;

    error:
    if (fd != NULL) {
        fclose(fd);
    }

    // esp_sd_card_unmount();

    // If we are in LOGTASK FWUPDATE state, we need to exit it else we will be stuck in it
    if (Logger_getState() == LOGTASK_FWUPDATE)
    {
       Logtask_fw_update_exit();
    }

    return ESP_FAIL;

}

/* Handler to delete a file from the server */
esp_err_t delete_post_handler(httpd_req_t *req)
{
    if (!rest_check_auth(req)) {
        return rest_send_auth_required(req);
    }

      if (Logger_getState() == LOGTASK_LOGGING){
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cannot delete files while logging");
        return ESP_FAIL;
    }

    char filepath[FILE_PATH_MAX];
    
    struct stat file_stat;

    //   // Strip out the /data part
    // const char *data_substr = "/data";
    // const char *data_pos = strstr(req->uri, data_substr);
    
    // // create new buffer
    // char *new_uri = malloc(strlen(req->uri) - strlen(data_substr) + 1);
    // if (new_uri == NULL) {
    //     ESP_LOGE(TAG_FILESERVER, "Failed to allocate memory for new_uri");
    //     return ESP_FAIL;
    // }

    // // copy the string up to the /data part
    // strncpy(new_uri, req->uri, data_pos - req->uri);

    // // copy the string after the /data part
    // strncpy(new_uri + (data_pos - req->uri), data_pos + strlen(data_substr), strlen(req->uri) - strlen(data_substr) - (data_pos - req->uri) + 1);

    // ESP_LOGI(TAG_FILESERVER, "%s", new_uri);

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, "/sdcard",
                                             req->uri + sizeof("/delete") - 1, sizeof(filepath));


    ESP_LOGI(TAG_FILESERVER, "filename: %s,  filepath: %s", filename, filepath);

    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG_FILESERVER, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG_FILESERVER, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    #ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "Deleting file : %s", filename);
    #endif

    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "Failed to mount SD card");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to mount SD card");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_FILESERVER, "%s", filepath);

    /* Delete file */
    if (unlink(filepath) != 0) {
        ESP_LOGE(TAG_FILESERVER, "Failed to delete %s (errno %d)", filepath, errno);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
        return ESP_FAIL;
    }

    // esp_sd_card_unmount();
    /* Redirect onto root to see the updated file list */
    // httpd_resp_set_status(req, "303 See Other");
    // httpd_resp_set_hdr(req, "Location", "/data");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "ack");
    return ESP_OK;
}


esp_err_t fwupdate_get_handler(httpd_req_t *req)
{
    if (!rest_check_auth(req)) {
        return rest_send_auth_required(req);
    }

    if (strstr(req->uri, "/fwupdate/enable") != NULL)
    {
        if (Logger_startFWupdate() != ESP_OK)
        {
            ESP_LOGE(TAG_FILESERVER, "Failed to start firmware update");
            httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Logger not idle, cannot update firwmare");
            return ESP_FAIL;
        } else {
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
            httpd_resp_send_chunk(req, NULL, 0);

            return ESP_OK;
        }
    }
   
   // Detect if request is pointing to /fwupdate/startupgrade
    if (strstr(req->uri, "/fwupdate/startupgrade") != NULL) {

        #ifdef DEBUG_FILESERVER
        ESP_LOGI(TAG_FILESERVER, "Starting firmware upgrade");
        #endif
        /* Send HTML file header */
        httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");
        
        httpd_resp_sendstr_chunk(req, "<p>Starting firmware upgrade...</p>");
        
        Logger_startFWflash();

        // error:
        httpd_resp_send_chunk(req, NULL, 0);
     
        return ESP_OK;
    
    }

    if (strstr(req->uri, "/fwupdate/state") != NULL) 
    {
        uint8_t state = Logger_getFWState();
        char buf[5];
        sprintf(buf, "%u", state);
        httpd_resp_sendstr_chunk(req, buf);
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;   
    }
   
   // /* Send HTML file header */
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

    /* Get handle to embedded file upload script */
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);
    httpd_resp_send_chunk(req, NULL, 0);


    return ESP_OK;


}
