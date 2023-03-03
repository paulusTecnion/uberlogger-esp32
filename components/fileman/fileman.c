#include "fileman.h"
#include "../../main/settings.h"
#include "../../main/config.h"

#define MOUNT_POINT "/sdcard"
static const char *TAG_FILE = "FILEMAN";
static uint32_t file_seq_num=0;
static FILE* f = NULL;
char file_name[16];
char filestrbuffer[200];

void fileman_create_filename()
{
    char filext[5];
    if (settings_get_logmode() == LOGMODE_CSV)
    {
        sprintf(filext, "%s", ".csv");
    } else {
        sprintf(filext, "%s", ".dat");
    }

    sprintf(file_name, MOUNT_POINT"/log%d%s", file_seq_num, filext);
    // ESP_LOGI(TAG_FILE, "%s", file_name);
}

// fileman_search_last_sequence_file must be called before callign this function!
esp_err_t fileman_open_file(void)
{
    if (strcmp(file_name, "") == 0)
    {
        #ifdef DEBUG_FILEMAN
        ESP_LOGI(TAG_FILE, "No file name specified!");
        #endif
        return ESP_FAIL;
    }
    fileman_create_filename();
    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "Opening file %s", file_name);
    #endif
    
    // Open file
    f = fopen(file_name, "w");
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

    return ESP_OK;
}

esp_err_t fileman_close_file(void)
{
    if (f!=NULL)
    {
        fclose(f);
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

esp_err_t fileman_search_last_sequence_file(void)
{
    f = NULL;
    file_seq_num = 0;

    
    while (1) {
        fileman_create_filename();
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
    ESP_LOGI(TAG_FILE, "Sequence number: %d", file_seq_num);
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
    return fprintf(f, "t,adc0,adc1,adc2,adc3,adc4,adc5,adc6,adc7,io0,io1,io2,io3,io4,io5\r\n");
    
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

    for (int i = 0; i<datarows; i++)
    {
        // Print time stamp
        writeptr = writeptr + sprintf(filestrbuffer, "20%d-%02d-%02d %02d:%02d:%02d.%03d,",
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
            
            if (dataAdc[i*NUM_ADC_CHANNELS+x] > -1000000 && dataAdc[i*NUM_ADC_CHANNELS+x]<0)
            {
                writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "-%d.%06d,",
                dataAdc[i*NUM_ADC_CHANNELS+x] / 1000000, abs((dataAdc[i*NUM_ADC_CHANNELS+x] - ((dataAdc[i*NUM_ADC_CHANNELS+x]/ 1000000)*1000000))));
            } else {
                writeptr = writeptr + snprintf(filestrbuffer+writeptr, 14, "%d.%06d,", 
                dataAdc[i*NUM_ADC_CHANNELS+x] / 1000000, abs((dataAdc[i*NUM_ADC_CHANNELS+x] - ((dataAdc[i*NUM_ADC_CHANNELS+x]/ 1000000)*1000000))));
            }


        }
        // Finally the IOs
        snprintf(filestrbuffer+writeptr, (2*6+5), "%d,%d,%d,%d,%d,%d\r\n",
            (dataGpio[j] & 0x04) && 1,
            (dataGpio[j] & 0x08) && 1,
            (dataGpio[j] & 0x10) && 1,
            (dataGpio[j] & 0x20) && 1,
            (dataGpio[j] & 0x40) && 1,
            (dataGpio[j] & 0x80) && 1);
        
        writeptr = 0;
 
        j++;
        // ESP_LOGI(TAG_FILE, "%s", filestrbuffer);
        if (fprintf(f, filestrbuffer) < 0)
        {
            return 0;
        }
        
    }

   return lenAdc;
}
