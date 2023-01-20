#include "fileman.h"
#include "../../main/settings.h"

#define MOUNT_POINT "/sdcard"
static const char *TAG_FILE = "FILEMAN";
static uint32_t file_seq_num=0;
static FILE* f = NULL;
char file_name[16];
char strbuffer[100];

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
    ESP_LOGI(TAG_FILE, "%s", file_name);
}

// fileman_search_last_sequence_file must be called before callign this function!
esp_err_t fileman_open_file(void)
{
    if (strcmp(file_name, "") == 0)
    {
        ESP_LOGI(TAG_FILE, "No file name specified!");
        return ESP_FAIL;
    }
    fileman_create_filename();
    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(TAG_FILE, "Opening file %s", file_name);

    
    // Open file
    f = fopen(file_name, "w");
    if (f == NULL) {
        ESP_LOGE(TAG_FILE, "Failed to open file for writing");
        return ESP_FAIL;
    }


    if (setvbuf(f, NULL, _IOFBF, 4096) != 0)
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
        ESP_LOGI(TAG_FILE, "Closing file");
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
    ESP_LOGI(TAG_FILE, "Sequence number: %d", file_seq_num);
    
   

    
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
    return len;
}

int fileman_csv_write_header()
{
    return fprintf(f, "adc0,adc1,adc2,adc3,adc4,adc5,adc6,adc7,io0,io1,io2,io3,io4,io5\r\n");
    
}

int fileman_csv_write(const void * dataAdc,  size_t lenAdc, const uint8_t* dataGpio, size_t lenGpio)
{
    
    int32_t* int32_data  = (int32_t*) dataAdc;
    int j =0;
    for (int i = 0; i<lenAdc; i=i+8)
    {
        // make string out of number
        //snprintf(strbuffer, sizeof(strbuffer), "%0d.%010llu,", (int32_data[i] >> 27), (((int32_data[i] & 0x7FFFFFF) * 100000000L)/(1 << 27) ));
        if (int32_data[i] > -1000000 && int32_data[i]<0)
        {
            snprintf(strbuffer, (8*11+2*6+1+1), "\-%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d,%d,%d,%d,%d,%d\r\n", 
            int32_data[i] / 1000000, abs((int32_data[i] - ((int32_data[i]/ 1000000)*1000000))),
            int32_data[i+1] / 1000000, abs((int32_data[i+1] - ((int32_data[i+1]/ 1000000)*1000000))),
            int32_data[i+2] / 1000000, abs((int32_data[i+2] - ((int32_data[i+2]/ 1000000)*1000000))),
            int32_data[i+3] / 1000000, abs((int32_data[i+3] - ((int32_data[i+3]/ 1000000)*1000000))),
            int32_data[i+4] / 1000000, abs((int32_data[i+4] - ((int32_data[i+4]/ 1000000)*1000000))),
            int32_data[i+5] / 1000000, abs((int32_data[i+5] - ((int32_data[i+5]/ 1000000)*1000000))),
            int32_data[i+6] / 1000000, abs((int32_data[i+6] - ((int32_data[i+6]/ 1000000)*1000000))),
            int32_data[i+7] / 1000000, abs((int32_data[i+7] - ((int32_data[i+7]/ 1000000)*1000000))),
            (dataGpio[j] & 0x04) && 1,
            (dataGpio[j] & 0x08) && 1,
            (dataGpio[j] & 0x10) && 1,
            (dataGpio[j] & 0x20) && 1,
            (dataGpio[j] & 0x40) && 1,
            (dataGpio[j] & 0x80) && 1
            );
        }
        else
        {
            snprintf(strbuffer, (8*11+2*6+1), "%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d,%d,%d,%d,%d,%d\r\n", 
            int32_data[i] / 1000000, abs((int32_data[i] - ((int32_data[i]/ 1000000)*1000000))),
            int32_data[i+1] / 1000000, abs((int32_data[i+1] - ((int32_data[i+1]/ 1000000)*1000000))),
            int32_data[i+2] / 1000000, abs((int32_data[i+2] - ((int32_data[i+2]/ 1000000)*1000000))),
            int32_data[i+3] / 1000000, abs((int32_data[i+3] - ((int32_data[i+3]/ 1000000)*1000000))),
            int32_data[i+4] / 1000000, abs((int32_data[i+4] - ((int32_data[i+4]/ 1000000)*1000000))),
            int32_data[i+5] / 1000000, abs((int32_data[i+5] - ((int32_data[i+5]/ 1000000)*1000000))),
            int32_data[i+6] / 1000000, abs((int32_data[i+6] - ((int32_data[i+6]/ 1000000)*1000000))),
            int32_data[i+7] / 1000000, abs((int32_data[i+7] - ((int32_data[i+7]/ 1000000)*1000000))),
            (dataGpio[j] & 0x04) && 1,
            (dataGpio[j] & 0x08) && 1,
            (dataGpio[j] & 0x10) && 1,
            (dataGpio[j] & 0x20) && 1,
            (dataGpio[j] & 0x40) && 1,
            (dataGpio[j] & 0x80) && 1
            );
        }


        // snprintf(strbuffer, (8*11+2*6+1), "%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d.%06d,%d,%d,%d,%d,%d,%d\n", 
        //     int32_data[i] / 1000000, abs((int32_data[i] - ((int32_data[i]/ 1000000)*1000000))),
        //     int32_data[i+1] / 1000000, abs((int32_data[i+1] - ((int32_data[i+1]/ 1000000)*1000000))),
        //     int32_data[i+2] / 1000000, abs((int32_data[i+2] - ((int32_data[i+2]/ 1000000)*1000000))),
        //     int32_data[i+3] / 1000000, abs((int32_data[i+3] - ((int32_data[i+3]/ 1000000)*1000000))),
        //     int32_data[i+4] / 1000000, abs((int32_data[i+4] - ((int32_data[i+4]/ 1000000)*1000000))),
        //     int32_data[i+5] / 1000000, abs((int32_data[i+5] - ((int32_data[i+5]/ 1000000)*1000000))),
        //     int32_data[i+6] / 1000000, abs((int32_data[i+6] - ((int32_data[i+6]/ 1000000)*1000000))),
        //     int32_data[i+7] / 1000000, abs((int32_data[i+7] - ((int32_data[i+7]/ 1000000)*1000000))),
        //     (dataGpio[j] & 0x04) && 1,
        //     (dataGpio[j] & 0x08) && 1,
        //     (dataGpio[j] & 0x10) && 1,
        //     (dataGpio[j] & 0x20) && 1,
        //     (dataGpio[j] & 0x40) && 1,
        //     (dataGpio[j] & 0x80) && 1
        //     );
       
           j++;
        // if ((i % 8) == 0 && (i!=0))
        // {
            //  fprintf(f,"\n");
         
        // }
        fprintf(f, strbuffer);
        
    }
    // Final new line
    // fprintf(f,"\n");
    
  
   return lenAdc;
}
