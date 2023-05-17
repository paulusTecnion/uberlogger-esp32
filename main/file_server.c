#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "file_server.h"
#include "esp_sd_card.h"
#include "firmware-www.h"
#include "firmwareESP32.h"
#include "firmwareSTM32.h"
#include "logger.h"


/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (1000*1024) // 1MB
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

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
// static esp_err_t index_html_get_handler(httpd_req_t *req)
// {
//     httpd_resp_set_status(req, "307 Temporary Redirect");
//     httpd_resp_set_hdr(req, "Location", "/");
//     httpd_resp_send(req, NULL, 0);  // Response body can be empty
//     return ESP_OK;
// }

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    // extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    // extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    // const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    // httpd_resp_set_type(req, "image/x-icon");
    // httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
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

    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "Cannot mount SD card");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot mount SD card");
        return ESP_FAIL;
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

    // /* Send HTML file header */
    // httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

    // /* Get handle to embedded file upload script */
    // extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    // extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    // const size_t upload_script_size = (upload_script_end - upload_script_start);

    // /* Add file upload form and script which on execution sends a POST request to /upload */
    // httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    // /* Send file-list table definition and column labels */
    // httpd_resp_sendstr_chunk(req,
    //     "<table class=\"fixed\" border=\"1\">"
    //     "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
    //     "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
    //     "<tbody>");

    /* Iterate over all files / folders and fetch their names and sizes */
    int i = 1;
    // create new root cjson
    
    httpd_resp_set_type(req, "application/json");
    cJSON * main = cJSON_CreateObject();
    if (main == NULL)
    {
        return ESP_FAIL;
    }

    cJSON * root = cJSON_AddObjectToObject(main, "root");
    char int2str[11];

    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "DIRECTORY" : "FILE");

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

        // httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
        // httpd_resp_sendstr_chunk(req, req->uri);
        // httpd_resp_sendstr_chunk(req, entry->d_name);
        // if (entry->d_type == DT_DIR) {
        //     httpd_resp_sendstr_chunk(req, "/");
        // }
        // httpd_resp_sendstr_chunk(req, "\">");
        // httpd_resp_sendstr_chunk(req, entry->d_name);
        // httpd_resp_sendstr_chunk(req, "</a></td><td>");
        // httpd_resp_sendstr_chunk(req, entrytype);
        // httpd_resp_sendstr_chunk(req, "</td><td>");
        // httpd_resp_sendstr_chunk(req, entrysize);
        // httpd_resp_sendstr_chunk(req, "</td><td>");
        // httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
        // httpd_resp_sendstr_chunk(req, req->uri);
        // httpd_resp_sendstr_chunk(req, entry->d_name);
        // httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
        // httpd_resp_sendstr_chunk(req, "</td></tr>\n");


    }
    closedir(dir);

    char* json_resp = cJSON_Print(main);
    // Send cJSON
    httpd_resp_send(req, json_resp, strlen(json_resp));

    free(json_resp);
    cJSON_Delete(main);

    // /* Finish the file list table */
    // httpd_resp_sendstr_chunk(req, "</tbody></table>");

    // /* Send remaining chunk of HTML file to complete it */
    // httpd_resp_sendstr_chunk(req, "</body></html>");

    // /* Send empty chunk to signal HTTP response completion */
    // httpd_resp_sendstr_chunk(req, NULL);
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
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
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

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
esp_err_t download_get_handler(httpd_req_t *req)
{
   
      if (Logger_getState() == LOGTASK_LOGGING){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot upload files while logging");
        return ESP_FAIL;
    }

    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
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
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
        return http_resp_dir_html(req, filepath);
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

    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "Failed to mount SD card");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to mount SD card");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG_FILESERVER, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }
    #ifdef DEBUG_FILESERVER
    ESP_LOGI(TAG_FILESERVER, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    #endif
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG_FILESERVER, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);

    esp_sd_card_unmount();
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
      if (Logger_getState() == LOGTASK_LOGGING){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot upload files while logging");
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
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG_FILESERVER, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
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
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    if (esp_sd_card_mount() != ESP_OK) {
        ESP_LOGE(TAG_FILESERVER, "Failed to mount SD card");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to mount SD card");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG_FILESERVER, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
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
            return ESP_FAIL;
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
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    esp_sd_card_unmount();
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
}

/* Handler to delete a file from the server */
esp_err_t delete_post_handler(httpd_req_t *req)
{
  
      if (Logger_getState() == LOGTASK_LOGGING){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot upload files while logging");
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
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG_FILESERVER, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG_FILESERVER, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
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
    unlink(filepath);

    esp_sd_card_unmount();
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
    esp_err_t err;

    if (strstr(req->uri, "/fwupdate/enable") != NULL)
    {
        if (Logger_startFWupdate() != ESP_OK)
        {
            ESP_LOGE(TAG_FILESERVER, "Failed to start firmware update");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Logger not idle, cannot update firwmare");
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
        // httpd_resp_sendstr_chunk(req, "<p>Flashing support chip ...(1/6)</p>");


       
        /* Start firmware upgrade */
        // if (flash_stm32() != ESP_OK) {
        //     ESP_LOGE(TAG_FILESERVER, "Support chip  failed!");
        //     httpd_resp_sendstr_chunk(req, "<p>Support chip  failed!</p>");
        //     goto error;
        // } else {
        //     #ifdef DEBUG_FILESERVER
        //     ESP_LOGI(TAG_FILESERVER, "Support chip flashed (2 / 6)");
        //     #endif
        //     httpd_resp_sendstr_chunk(req, "<p>Support chip flashed (2 / 6)</p>");
        // }

        // httpd_resp_sendstr_chunk(req, "<p>Flashing file system ...(3 / 6)</p>");
        // if (update_www() != ESP_OK) {
        //     ESP_LOGE(TAG_FILESERVER, "File system flash failed");
        //     httpd_resp_sendstr_chunk(req, "<p>File system flash failed!</p>");
        //     goto error;
        // } else {
        //     #ifdef DEBUG_FILESERVER
        //     ESP_LOGI(TAG_FILESERVER, "File system flashed (4 / 6)");
        //     #endif
        //     httpd_resp_sendstr_chunk(req, "<p>File system flashed (4 / 6)</p>");
        // }
        
        // httpd_resp_sendstr_chunk(req, "<p>Flashing main chip ...(5 / 6)</p>");
        // httpd_resp_sendstr_chunk(req, "<p>WiFi will be disabled and should re-enable again. If upgrade failed it will be shown here.</p>");
        
        
        // if (updateESP32() != ESP_OK) {
        //     ESP_LOGE(TAG_FILESERVER, "Main chip flash failed");
        //     httpd_resp_sendstr_chunk(req, "<p>Main chip flash failed! Please don't reset your Uberlogger and try again</p>");
        //     httpd_resp_send_chunk(req, NULL, 0);
        //     goto error;
        // } else {
        //     #ifdef DEBUG_FILESERVER
        //     ESP_LOGI(TAG_FILESERVER, "Main flash chip flashed (6 / 6)");
        //     #endif
        //     httpd_resp_sendstr_chunk(req, "<p>Main flash chip flashed (6 / 6)</p>");
        // }

        // httpd_resp_sendstr_chunk(req, "succesfull");
        // httpd_resp_send_chunk(req, NULL, 0);

        // error:
        httpd_resp_send_chunk(req, NULL, 0);

        
        // Reboot to apply firmware update
        // Logging_restartSystem();
        // xSemaphoreGive(idle_state);


     
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
