#include <stdio.h>
#include <string.h>
#include <time.h>
#include "fileman.h"
#include "../../main/settings.h"
#include "../../main/config.h"


#define MOUNT_POINT "/sdcard"
static const char *TAG_FILE = "FILEMAN";
static uint32_t file_seq_num=0;
static uint32_t file_seq_subnum=0;
static FILE* f = NULL;
char file_name[50];
char filestrbuffer[4900];
char _prefix[100];
uint32_t file_bytes_written = 0;

void fileman_create_filename(const char * prefix)
{
    char filext[5];
    if (settings_get_logmode() == LOGMODE_CSV)
    {
        sprintf(filext, "%s", ".csv");
    } else {
        sprintf(filext, "%s", ".dat");
    }

    // sprintf(file_name, MOUNT_POINT"/%s%d%s", prefix, file_seq_num, filext);
    sprintf(file_name, MOUNT_POINT"/%s%s", prefix, filext);
    #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "%s", file_name);
    #endif
}


uint8_t fileman_check_current_file_size(size_t sizeInBytes)
{
    if (file_bytes_written > sizeInBytes)
    {
         #ifdef DEBUG_FILEMAN
        ESP_LOGI(TAG_FILE, "File size exceeded %d bytes", sizeInBytes);
        #endif
        file_bytes_written = 0;
        return 1;
    } else {
        return 0;
    }
}

void fileman_reset_subnum(void)
{
    file_seq_subnum = 0;
}

esp_err_t fileman_set_prefix(const char * prefix, time_t timestamp)
{
      struct tm *tm_info;
    char time_buffer[20]; // Buffer to hold the formatted date and time

    if ( strlen(prefix) > (sizeof(_prefix) - sizeof(time_buffer)))
    {
        return ESP_ERR_INVALID_SIZE;
    }

  
    timestamp /= 1000;
    // Convert Unix timestamp to local time
    tm_info = localtime(&timestamp);

    // Format the time "YYYYMMDD_HH-MM-SS" suitable for filenames
    strftime(time_buffer, sizeof(time_buffer), "%Y%m%d_%H-%M-%S", tm_info);


    snprintf(_prefix, sizeof(_prefix), "%s_%s",time_buffer, prefix);

// fileman_search_last_sequence_file
    // Determine last index of file with prefix
    // uint32_t t= fileman_search_last_sequence_file();
     #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "Prefix set: %s", _prefix);
    #endif
    return ESP_OK;
}
    

// fileman_search_last_sequence_file must be called before callign this function!
esp_err_t fileman_open_file(time_t timestamp)
{
    fileman_create_filename(_prefix);
    if (strcmp(file_name, "") == 0)
    {
        #ifdef DEBUG_FILEMAN
        ESP_LOGI(TAG_FILE, "No file name specified!");
        #endif
        return ESP_FAIL;
    }

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "Opening file %s", file_name);
    #endif
    
    // Open file
    f = fopen(file_name, "a");
    if (f == NULL) {
        ESP_LOGE(TAG_FILE, "Failed to open file for writing");
        return ESP_FAIL;
    }


    if (setvbuf(f, NULL, _IOFBF, 8192) != 0)
    {
        ESP_LOGE(TAG_FILE, "setvbuf failed");
        perror ("The following error occurred");
        return ESP_FAIL;
    }

    // All good, set written bytes to 0
    file_bytes_written = 0;

    return ESP_OK;
}

esp_err_t fileman_close_file(void)
{
    if (f!=NULL)
    {
        fclose(f);
        f = NULL;
        #ifdef DEBUG_FILEMAN
        ESP_LOGI(TAG_FILE, "Closing file");
        #endif
        file_seq_num++;
        return ESP_OK;
    } else {
        ESP_LOGE(TAG_FILE, "Cannot close file! f == NULL");
        return ESP_FAIL;
    }
}

uint32_t fileman_search_last_sequence_file(void)
{
    f = NULL;
    file_seq_num = 0;

    
    while (1) {
        fileman_create_filename(_prefix);
        f = fopen(file_name, "r");
    
        if (f==NULL)
        {
            break;
        } else {
            fclose(f); 
        }

        file_seq_num++;
        

    }
    #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "File name number: %d", file_seq_num);
    #endif

    return file_seq_num;
}
 

int fileman_write(const void * data, size_t len)
{
    size_t write_result = fwrite(data,len, 1, f);
    if (write_result != 1)
    {
        ESP_LOGE(TAG_FILE,"Error writing to backing store %d %d\n", (int)len, write_result);
        perror ("The following error occurred");
        return 0;
    }
    return write_result;
}

int fileman_csv_write_header()
{
    // first write entire string into buffer
    char filestrbuffer[200];
    int writeptr = 0;
    writeptr = writeptr + sprintf(filestrbuffer+writeptr, "time(utc),");
    // Print ADC or NTC, depending on settings
    for (int i = 0; i<NUM_ADC_CHANNELS; i++)
    {
        if ((settings_get()->adc_channel_type & (1 << i)))
        {
            writeptr = writeptr + sprintf(filestrbuffer+writeptr, "NTC%d,", i+1);
        } else {
            writeptr = writeptr + sprintf(filestrbuffer+writeptr, "AIN%d,", i+1);
        }
    }
    writeptr = writeptr + sprintf(filestrbuffer+writeptr,"DI1,DI2,DI3,DI4,DI5,DI6\r\n");
    // ESP_LOGI(TAG_FILE, "%s", filestrbuffer);
    // finally write to file and return
    return fprintf(f, filestrbuffer);

  
    
}

int fileman_csv_write(const int32_t * dataAdc,  size_t lenAdc, const uint8_t* dataGpio, size_t lenGpio, const uint8_t* dataTime, size_t lenTime, size_t datarows)
{
    int j = 0;
    uint32_t writeptr =0;
    
    s_date_time_t *date_time_ptr = (s_date_time_t*)dataTime;
    // ESP_LOGI(TAG_FILE,"Lengths: %d, %d, %d", lenAdc, lenGpio, lenTime);
    
    // for (int i = 0; i<32; i++)
    // {
    //     ESP_LOGI(TAG_FILE, "ADC fp: %d", *(dataAdc+i));
    // }

    // ESP_LOGI(TAG_FILE, "Writing %d rows", datarows);

    for (int i = 0; i<datarows; i++)
    {
        // Print time stamp
        writeptr = writeptr + sprintf(filestrbuffer+writeptr, "20%d-%02d-%02d %02d:%02d:%02d.%03lu,",
            date_time_ptr[j].year, 
            date_time_ptr[j].month,
            date_time_ptr[j].date,
            date_time_ptr[j].hours,
            date_time_ptr[j].minutes,
            date_time_ptr[j].seconds,
            date_time_ptr[j].subseconds);
        
      
        // Print ADC
        for (int x=0; x<NUM_ADC_CHANNELS; x++)
        {
            
            // if (dataAdc[i*NUM_ADC_CHANNELS+x] > -1000000 && dataAdc[i*NUM_ADC_CHANNELS+x]<0)
            // {
            //     writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "-%d.%06d,",
            //     dataAdc[i*NUM_ADC_CHANNELS+x] / 1000000, abs((dataAdc[i*NUM_ADC_CHANNELS+x] - ((dataAdc[i*NUM_ADC_CHANNELS+x]/ 1000000)*1000000))));
            // } else {
            //     writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "%d.%06d,", 
            //     dataAdc[i*NUM_ADC_CHANNELS+x] / 1000000, abs((dataAdc[i*NUM_ADC_CHANNELS+x] - ((dataAdc[i*NUM_ADC_CHANNELS+x]/ 1000000)*1000000))));
            // }

            // If temperature sensor and 16 bits we need to multiply input with 100 (or divide with factor 100 less which is ADC_MULT_FACTOR_16B_TEMP)
            if ((settings_get()->adc_channel_type & (1 << x)))
            {
                    // Only 2 digits after decimal point for temperature
                    writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "%s%d.%06d,",
                        (dataAdc[i*NUM_ADC_CHANNELS+x] < 0) ? "-" : "",
                        abs(dataAdc[i*NUM_ADC_CHANNELS+x] / (ADC_MULT_FACTOR_16B_TEMP)), 
                        abs(dataAdc[i*NUM_ADC_CHANNELS+x] % ADC_MULT_FACTOR_16B_TEMP));
                // } else {
                //     writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "%s%d.%06d,",
                //         (dataAdc[i*NUM_ADC_CHANNELS+x] < 0) ? "-" : "",
                //         abs(dataAdc[i*NUM_ADC_CHANNELS+x] / (ADC_MULT_FACTOR_12B_TEMP)), 
                //         abs(dataAdc[i*NUM_ADC_CHANNELS+x] % ADC_MULT_FACTOR_12B_TEMP));
                // }
            } else {
                // if range is 60V...
                if ((settings_get()->adc_channel_range & (1<<x)))
                {
                    // We need 6 digits after comma for +/-60V
                    writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "%s%d.%06d,",
                        (dataAdc[i*NUM_ADC_CHANNELS+x] < 0) ? "-" : "",
                        abs(dataAdc[i*NUM_ADC_CHANNELS+x] / (ADC_MULT_FACTOR_60V)), 
                        abs(dataAdc[i*NUM_ADC_CHANNELS+x] % ADC_MULT_FACTOR_60V));
                } else {
                    // We need 7 digits after comma for +/-10V
                    writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "%s%d.%07d,",
                        (dataAdc[i*NUM_ADC_CHANNELS+x] < 0) ? "-" : "",
                        abs(dataAdc[i*NUM_ADC_CHANNELS+x] / (ADC_MULT_FACTOR_10V)), 
                        abs(dataAdc[i*NUM_ADC_CHANNELS+x] % ADC_MULT_FACTOR_10V));
                }
            }
            
         
            
        }

        

        // Finally the IOs
        writeptr = writeptr + snprintf(filestrbuffer+writeptr, (2*6+5), "%d,%d,%d,%d,%d,%d\r\n",
            (dataGpio[j] & 0x04) && 1,
            (dataGpio[j] & 0x08) && 1,
            (dataGpio[j] & 0x10) && 1,
            (dataGpio[j] & 0x20) && 1,
            (dataGpio[j] & 0x40) && 1,
            (dataGpio[j] & 0x80) && 1);
        // ESP_LOGI(TAG_FILE, "%d %ld", j, writeptr);
        
        // writeptr = 0;
 
        j++;
       
    //    if (j % 35 == 0)
    //    {
        // replace by put function. Much faster

        // filestrbuffer[writeptr] = '\0';
        
        if (f == NULL)
        {
            return 0;
        }

        // int len = fprintf(f, (const char*)filestrbuffer);
        int len = fputs((const char*)filestrbuffer, f);
        // if (fputs((const char*)filestrbuffer, f) < 0)
        if (len < 0)
        {    
            return 0;
        }
    
        file_bytes_written += strlen(filestrbuffer);
        writeptr = 0;
    //    }

    }

    ESP_LOGI(TAG_FILE, "%d", file_bytes_written);

    // Write any remaining data
    // if (j % 35 != 0)
    // {
    //     // replace by put function. Much faster
    //     // int len = strlen((const char*)filestrbuffer);
    //     filestrbuffer[writeptr] = '\0';
    //     int len = fprintf(f, (const char*)filestrbuffer);
    //     // int len = fputs((const char*)filestrbuffer, f);
    //     // if (fputs((const char*)filestrbuffer, f) < 0)
    //     if (len < 0)
    //     {    
    //         return 0;
    //     }
    //     file_bytes_written += writeptr;
    //     writeptr = 0;
    // }
    // ESP_LOGI(TAG_FILE, "%d %s %ld", datarows, filestrbuffer, writeptr);

    

   return lenAdc;
}
