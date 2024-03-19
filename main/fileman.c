#include <stdio.h>
#include <string.h>
#include <time.h>
#include "fileman.h"
#include "settings.h"
#include "config.h"


#define MOUNT_POINT "/sdcard"
static const char *TAG_FILE = "FILEMAN";
static uint32_t file_seq_num=0;
static uint32_t file_seq_subnum=0;
static FILE* f = NULL;
char _file_name[105];
char filestrbuffer[4900];
char _prefix[100];
uint32_t file_bytes_written = 0;
uint8_t raw_file_format_version = RAW_FILE_FORMAT_VERSION;
void fileman_create_filename(const char * prefix, char * file_name)
{
    char filext[5];
    if (settings_get_logmode() == LOGMODE_CSV)
    {
        sprintf(filext, "%s", ".csv");
    } else {
        sprintf(filext, "%s", ".dat");
    }

    // if (settings_get_file_name_mode() == FILE_NAME_MODE_SEQ_NUM)
    // {
    //     sprintf(file_name, MOUNT_POINT"/%s%d%s", prefix, file_seq_num, filext);
    // } else {
    sprintf(file_name, MOUNT_POINT"/%s%s", prefix, filext);
    // }
    
    
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

esp_err_t fileman_set_prefix(const char * prefix, time_t timestamp, uint8_t continueNumbering)
{
      struct tm *tm_info;
    char time_buffer[20]; // Buffer to hold the formatted date and time

    if ( strlen(prefix) > (sizeof(_prefix) - sizeof(time_buffer)))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    if(settings_get_file_name_mode() == FILE_NAME_MODE_SEQ_NUM)
    {
        if (continueNumbering)
        {
            snprintf(_prefix, sizeof(_prefix), "%s%ld", prefix, file_seq_num);
        } else {
            snprintf(_prefix, sizeof(_prefix), "%s%ld", prefix, fileman_search_last_sequence_file(prefix));
        }
    } else {
  
        timestamp /= 1000;
        // Convert Unix timestamp to local time
        tm_info = localtime(&timestamp);

        // Format the time "YYYYMMDD_HH-MM-SS" suitable for filenames
        strftime(time_buffer, sizeof(time_buffer), "%Y%m%d_%H-%M-%S", tm_info);


        snprintf(_prefix, sizeof(_prefix), "%s_%s",time_buffer, prefix);
    }

     #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "Prefix set: %s", _prefix);
    #endif
    return ESP_OK;
}
    

// fileman_search_last_sequence_file must be called before callign this function!
esp_err_t fileman_open_file()
{
    // Create the file name based on the previously called fileman_set_prefix function
    fileman_create_filename(_prefix, _file_name);

    if (strcmp(_file_name, "") == 0)
    {
        #ifdef DEBUG_FILEMAN
        ESP_LOGI(TAG_FILE, "No file name specified!");
        #endif
        return ESP_FAIL;
    }

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "Opening file %s", _file_name);
    #endif
    
    // Open file
    f = fopen(_file_name, "a");
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
        if (settings_get_file_name_mode() == FILE_NAME_MODE_SEQ_NUM)
        {
            file_seq_num++;
        }
        return ESP_OK;
    } else {
        ESP_LOGE(TAG_FILE, "Cannot close file! f == NULL");
        return ESP_FAIL;
    }
}

uint32_t fileman_search_last_sequence_file(const char * prefix)
{
    f = NULL;
    file_seq_num = 0;

    char tmpFileName[105];
    char tmpPrefix[100];

    while (1) {
        
        snprintf(tmpPrefix, sizeof(tmpPrefix), "%s%ld", prefix, file_seq_num);
        fileman_create_filename(tmpPrefix, tmpFileName);

        f = fopen(tmpFileName, "r");
    
        if (f==NULL)
        {
            break;
        } else {
            fclose(f); 
        }

        file_seq_num++;
        

    }
    #ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "File name number: %ld", file_seq_num);
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
    file_bytes_written = file_bytes_written + len;
    return write_result;
}

int fileman_csv_write_header()
{
    // first write entire string into buffer
    char filestrbuffer[200];
    int writeptr = 0;
    writeptr = writeptr + sprintf(filestrbuffer + writeptr, "time(utc),");
    // Print ADC or NTC, depending on settings
    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        if (!settings_get_adc_channel_enabled(i))
        {
            continue;
        }   

        if ((settings_get()->adc_channel_type & (1 << i)))
        {
            writeptr = writeptr + sprintf(filestrbuffer + writeptr, "NTC%d%s", i + 1, (i == settings_get_last_enabled_ADC_channel() && (settings_get_last_enabled_GPIO_channel() == -1)) ? "" : ",");
        }
        else
        {
            writeptr = writeptr + sprintf(filestrbuffer + writeptr, "AIN%d%s", i + 1, (i == settings_get_last_enabled_ADC_channel() && (settings_get_last_enabled_GPIO_channel() == -1)) ? "" : ",");
        }
    }

     // Finally the IOs
    for (int x = 0; x < 6; x++)
    {
        if (settings_get_gpio_channel_enabled(x))
        {
            // Assuming MAX_LENGTH is the total size of filestrbuffer.
            // Replace MAX_LENGTH with the actual size of filestrbuffer.
            int remaining_space = 200 - writeptr;
            writeptr += snprintf(filestrbuffer + writeptr, remaining_space, "DI%d%s", x+1, (x == settings_get_last_enabled_GPIO_channel()) ? "" : ",");
        }
    }

    // Make sure there is enough room for "\r\n" and the null terminator.
    int remaining_space = 200 - writeptr;
    if (remaining_space >= 3) // Enough space for "\r\n" and null-terminator.
    {
        writeptr += snprintf(filestrbuffer + writeptr, remaining_space, "\r\n");
    }
    // ESP_LOGI(TAG_FILE, "%s", filestrbuffer);
    // finally write to file and return
    return fprintf(f, filestrbuffer);
}

esp_err_t fileman_csv_write_spi_msg(sdcard_data_t *sdcard_data, const int32_t *adcData)
{
    // loop through the sdcard data and write to file

    int32_t ret = 0;
    uint16_t i = 0;
    // Determine how many spi_msg_t data we have
    // int numSpiMsgs = sdcard_data->datarows / DATA_LINES_PER_SPI_TRANSACTION;


    spi_msg_1_t *t_spi_msg_1;// = (spi_msg_1_t *)(sdcard_data->spi_data);
    spi_msg_2_t *t_spi_msg_2;// = (spi_msg_2_t *)(sdcard_data->spi_data);
    // ret = fileman_csv_write(adcData,  spi_msg->gpioData,  spi_msg->timeData, spi_msg->dataLen);
        for (i = 0; i < sdcard_data->numSpiMessages; i++)
        {
            // Depending on which data we are writing, we need to use a different pointer
            // if (i % 2 != 0)
            // {
            //     // ESP_LOGI(TAG_FILE, "Writing SPI msg 2");
            //     spi_msg_2_adc_only_t *spi_msg = (spi_msg_2_adc_only_t *)(sdcard_data->spi_data + i * sizeof(spi_msg_2_adc_only_t));
            //     ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, ADC_VALUES_PER_SPI_TRANSACTION, spi_msg->dataLen);
            // }
            // else
            // {
            //     //  ESP_LOGI(TAG_FILE, "Writing SPI msg 1, i: %u, numSpiMsgs: %d, ", i, numSpiMsgs);
            //     spi_msg_1_adc_only_t *spi_msg = (spi_msg_1_adc_only_t *)(sdcard_data->spi_data + i * sizeof(spi_msg_1_adc_only_t));
            //     ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, ADC_VALUES_PER_SPI_TRANSACTION, spi_msg->dataLen);
            // }
            if (i % 2 != 0 )
            {
                t_spi_msg_2 = (spi_msg_2_t *)(sdcard_data->spi_data + i * sizeof(spi_msg_2_t));
                ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, t_spi_msg_2->gpioData, t_spi_msg_2->timeData, t_spi_msg_2->dataLen);
            } else 
            {
                t_spi_msg_1 = (spi_msg_1_t *)(sdcard_data->spi_data + i * sizeof(spi_msg_1_t));
                ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, t_spi_msg_1->gpioData, t_spi_msg_1->timeData, t_spi_msg_1->dataLen);
            }
           
            
            // Abort mission when failure occured..
            if (ret < 0)
                return ESP_FAIL;
        }

        // if (numRemainingRows > 0)
        // {
        //     // Depending on which data we are writing, we need to use a different pointer
        //     // if ((i % 2) != 0)
        //     // {
        //     //     // ESP_LOGI(TAG_FILE, "Writing remaining rows 2");
        //     //     spi_msg_2_adc_only_t *spi_msg = (spi_msg_2_adc_only_t *)(sdcard_data->spi_data + (i) * sizeof(spi_msg_2_adc_only_t));
        //     //     ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, ADC_BYTES_PER_SPI_TRANSACTION, spi_msg->dataLen);
        //     // }
        //     // else
        //     // {
        //     //     //  ESP_LOGI(TAG_FILE, "Writing remaining rows 1");
        //     //     spi_msg_1_adc_only_t *spi_msg = (spi_msg_1_adc_only_t *)(sdcard_data->spi_data + (i) * sizeof(spi_msg_1_adc_only_t));
        //     //     ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, ADC_BYTES_PER_SPI_TRANSACTION, spi_msg->dataLen);
        //     // }
        //     if ((i == 0) || (i % 2 != 0))
        //     {
        //         t_spi_msg_1 = (spi_msg_1_t *)(sdcard_data->spi_data + (i) * sizeof(spi_msg_1_t));
        //         ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, t_spi_msg_1->gpioData, t_spi_msg_1->timeData, t_spi_msg_1->dataLen);
        //     } else {
        //         t_spi_msg_2 = (spi_msg_2_t *)(sdcard_data->spi_data + (i) * sizeof(spi_msg_2_t));
        //         ret = fileman_csv_write(adcData + i * ADC_VALUES_PER_SPI_TRANSACTION, t_spi_msg_2->gpioData, t_spi_msg_2->timeData, t_spi_msg_2->dataLen); 
        //     }
            
           
        // }
    

    return ESP_OK;
}

int fileman_csv_write(const int32_t *dataAdc, const uint8_t *dataGpio,  const s_date_time_t *date_time_ptr, size_t datarows)
{
    int j = 0;
    uint32_t writeptr = 0;

    for (int i = 0; i < datarows; i++)
    {
        // Print time stamp
        writeptr = writeptr + sprintf(filestrbuffer + writeptr, "20%02d-%02d-%02d %02d:%02d:%02d.%03lu,",
                                      date_time_ptr[j].year,
                                      date_time_ptr[j].month,
                                      date_time_ptr[j].date,
                                      date_time_ptr[j].hours,
                                      date_time_ptr[j].minutes,
                                      date_time_ptr[j].seconds,
                                      date_time_ptr[j].subseconds);

        // Print ADC
        for (int x = 0; x < NUM_ADC_CHANNELS; x++)
        {
            // Check if the channel is enable, else skip it
            if (!settings_get_adc_channel_enabled(x))
            {
                continue;
            }

             // Determine comma
            const char * comma = (i == settings_get_last_enabled_ADC_channel() && (settings_get_last_enabled_GPIO_channel() == -1)) ? "" : ",";

            // If temperature sensor and 16 bits we need to multiply input with 100 (or divide with factor 100 less which is ADC_MULT_FACTOR_16B_TEMP)
            if ((settings_get()->adc_channel_type & (1 << x)))
            {
                // Only 2 digits after decimal point for temperature
                writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d.%01d%s",
                                               (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                               abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_16B_TEMP)),
                                               abs((dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_16B_TEMP) / (ADC_MULT_FACTOR_16B_TEMP / 1000)), comma);
            }
            else
            {
               
                // if range is 60V...
                if ((settings_get()->adc_channel_range & (1 << x)))
                {
                    // We need 6 digits after comma for +/-60V
                    writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d.%06d%s",
                                                   (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_60V)),
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_60V), comma);
                }
                else
                {
                    // We need 7 digits after comma for +/-10V
                    writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d.%07d%s",
                                                   (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_10V)),
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_10V), comma);
                }
            }
        }

        // Finally the IOs
        for (int x = 0; x < 6; x++)
        {
            if (settings_get_gpio_channel_enabled(x))
            {
                writeptr = writeptr + snprintf(filestrbuffer + writeptr, 3, "%d%s",
                                       (dataGpio[j] & (0x01 >> x)) && 1,
                                        (x == settings_get_last_enabled_GPIO_channel()) ? "" : ",");
                           
            }
        }

        writeptr = writeptr + snprintf(filestrbuffer + writeptr, 3, "\r\n");
        
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

        // int len = fprintf(f, (const char *)filestrbuffer);
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

    return datarows;
}


esp_err_t fileman_raw_write_header()
{
    int w = 0;
    // Write raw format version number
    w = fwrite(&raw_file_format_version, sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    // first write ranges
    w = fwrite(&(settings_get()->adc_channel_range), sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    // then write ADC modes
    w = fwrite(&(settings_get()->adc_channel_type), sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    // which channels are enabled
    w = fwrite(&(settings_get()->adc_channels_enabled), sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    // then resolution
    w = fwrite(&(settings_get()->adc_resolution), sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    //  sample rate
    w = fwrite(&(settings_get()->adc_log_sample_rate), sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    // Write gpio channels enabled
    w = fwrite(&(settings_get()->gpio_channels_enabled), sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    // and don't forget the calibration data
    w = fwrite(settings_get_adc_offsets(), sizeof(int32_t), NUM_ADC_CHANNELS, f);
    if (w != sizeof(int32_t)*NUM_ADC_CHANNELS)
        return ESP_FAIL;  

    return ESP_OK;
}