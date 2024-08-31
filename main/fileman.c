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
char _file_name[120];
char filestrbuffer[4900];
char _prefix[105];
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

    char tmpFileName[120];
    char tmpPrefix[105];

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
    const char *separator_char = (settings_get_file_separator_char() == FILE_SEPARATOR_CHAR_COMMA) ? "," : ";";
    writeptr = writeptr + sprintf(filestrbuffer + writeptr, "time(utc)%s", separator_char);
    // Print ADC or NTC, depending on settings
    char buff[22];
    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        if (!settings_get_adc_channel_enabled(i))
        {
            continue;
        }   

        // if ((settings_get()->adc_channel_type & (1 << i)))
        // {
        //     writeptr = writeptr + sprintf(filestrbuffer + writeptr, "NTC%d%s", i + 1, (i == settings_get_last_enabled_ADC_channel() && (settings_get_last_enabled_GPIO_channel() == -1)) ? "" : separator_char);
        // }
        // else
        // {
        settings_get_ain_chan_label(i, buff);
        writeptr = writeptr + sprintf(filestrbuffer + writeptr, "%s%s", buff, (i == settings_get_last_enabled_ADC_channel() && (settings_get_last_enabled_GPIO_channel() == -1)) ? "" : separator_char);
        // }
    }

     // Finally the IOs
    for (int x = 0; x < NUM_DIO_CHANNELS; x++)
    {
        if (settings_get_gpio_channel_enabled(settings_get(), x))
        {
            // Assuming MAX_LENGTH is the total size of filestrbuffer.
            // Replace MAX_LENGTH with the actual size of filestrbuffer.
            int remaining_space = 200 - writeptr;
            settings_get_dio_chan_label(x, buff);
            writeptr += snprintf(filestrbuffer + writeptr, remaining_space, "%s%s", buff, (x == settings_get_last_enabled_GPIO_channel()) ? "" : separator_char);
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

    spi_msg_1_t *t_spi_msg_1;// = (spi_msg_1_t *)(sdcard_data->spi_data);
    spi_msg_2_t *t_spi_msg_2;// = (spi_msg_2_t *)(sdcard_data->spi_data);
    // ret = fileman_csv_write(adcData,  spi_msg->gpioData,  spi_msg->timeData, spi_msg->dataLen);
        for (i = 0; i < sdcard_data->numSpiMessages; i++)
        {
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

    return ESP_OK;
}

int fileman_csv_write(const int32_t *dataAdc, const uint8_t *dataGpio,  const s_date_time_t *date_time_ptr, size_t datarows)
{
    int j = 0;
    uint32_t writeptr = 0;

    const char *decimal_char = (settings_get_file_decimal_char() == FILE_DECIMAL_CHAR_COMMA) ? "," : ".";
    const char *separator_char = (settings_get_file_separator_char() == FILE_SEPARATOR_CHAR_COMMA) ? "," : ";";

    for (int i = 0; i < datarows; i++)
    {
        // Print time stamp
        writeptr = writeptr + sprintf(filestrbuffer + writeptr, "20%02d-%02d-%02d %02d:%02d:%02d.%03lu%s",
                                      date_time_ptr[j].year,
                                      date_time_ptr[j].month,
                                      date_time_ptr[j].date,
                                      date_time_ptr[j].hours,
                                      date_time_ptr[j].minutes,
                                      date_time_ptr[j].seconds,
                                      date_time_ptr[j].subseconds,
                                      separator_char);

        // Print ADC
        for (int x = 0; x < NUM_ADC_CHANNELS; x++)
        {
            // Check if the channel is enable, else skip it
            if (!settings_get_adc_channel_enabled(x))
            {
                continue;
            }

             // Determine comma
            const char * comma = (i == settings_get_last_enabled_ADC_channel() && (settings_get_last_enabled_GPIO_channel() == -1)) ? "" : separator_char;

            // If temperature sensor and 16 bits we need to multiply input with 100 (or divide with factor 100 less which is ADC_MULT_FACTOR_16B_TEMP)
            if ((settings_get()->adc_channel_type & (1 << x)))
            {
                // Only 2 digits after decimal point for temperature
                writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d%s%01d%s",
                                               (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                               abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_16B_TEMP)),
                                               decimal_char,
                                               abs((dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_16B_TEMP)), 
                                               comma);
            }
            else
            {
               
                // if range is 60V...
                if ((settings_get()->adc_channel_range & (1 << x)))
                {
                    // We need 6 digits after comma for +/-60V
                    writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d%s%06d%s",
                                                   (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_60V)),
                                                   decimal_char,
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_60V), 
                                                   comma);
                }
                else
                {
                    // We need 7 digits after comma for +/-10V
                    writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d%s%07d%s",
                                                   (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_10V)),
                                                   decimal_char,
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_10V), 
                                                   comma);
                }
            }
        }

        // Finally the IOs
        for (int x = 0; x < 6; x++)
        {
            if (settings_get_gpio_channel_enabled(settings_get(), x))
            {
                writeptr = writeptr + snprintf(filestrbuffer + writeptr, 3, "%d%s",
                                       (dataGpio[j] & (0x04 << x)) && 1,
                                        (x == settings_get_last_enabled_GPIO_channel()) ? "" : separator_char);
                           
            }
        }

        writeptr = writeptr + snprintf(filestrbuffer + writeptr, 3, "\r\n");

        j++;

        if (f == NULL)
        {
            return 0;
        }

        // int len = fprintf(f, (const char *)filestrbuffer);
        int len = fputs((const char*)filestrbuffer, f);
        vTaskDelay(pdTICKS_TO_MS(0));
        // if (fputs((const char*)filestrbuffer, f) < 0)
        if (len < 0)
        {
            return len;
        }
        file_bytes_written += strlen(filestrbuffer);
        writeptr = 0;
        //    }
    }

    return datarows;
}

esp_err_t fileman_raw_write_header(void)
{
    // First write the header to a memory buffer, because fseek is causing problems with sd cards.

    uint8_t header_buffer[512]; // Allocate a buffer large enough to hold the header
    uint32_t header_length = 0;
    char channel_label[MAX_CHANNEL_NAME_LEN]; // Assuming MAX_CHANNEL_NAME_LEN is defined elsewhere

    // Ensure buffer size is sufficient
    if (sizeof(header_buffer) < sizeof(uint32_t) + sizeof(uint8_t) * 9 + sizeof(int32_t) * NUM_ADC_CHANNELS + (sizeof(uint8_t) + MAX_CHANNEL_NAME_LEN) * (NUM_ADC_CHANNELS + NUM_DIO_CHANNELS))
    {
        ESP_LOGE(TAG_FILE, "Header buffer size is insufficient");
        return ESP_FAIL;
    }

    // Reserve space for header length (will write it later)
    uint8_t *ptr = header_buffer;
    ptr += sizeof(uint32_t);

    // Write raw format version number
    *ptr++ = raw_file_format_version;
    header_length += sizeof(uint8_t);

    // Write ranges
    *ptr++ = settings_get()->adc_channel_range;
    header_length += sizeof(uint8_t);

    // Write ADC modes
    *ptr++ = settings_get()->adc_channel_type;
    header_length += sizeof(uint8_t);

    // Write which channels are enabled
    *ptr++ = settings_get()->adc_channels_enabled;
    header_length += sizeof(uint8_t);

    // Write resolution
    *ptr++ = settings_get()->adc_resolution;
    header_length += sizeof(uint8_t);

    // Write sample rate
    *ptr++ = settings_get()->adc_log_sample_rate;
    header_length += sizeof(uint8_t);

    // Write GPIO channels enabled
    *ptr++ = settings_get()->gpio_channels_enabled;
    header_length += sizeof(uint8_t);

    // Write CSV decimal character option
    *ptr++ = settings_get()->file_decimal_char;
    header_length += sizeof(uint8_t);

    // Write CSV separator option
    *ptr++ = settings_get()->file_separator_char;
    header_length += sizeof(uint8_t);

    // Write the calibration data
    memcpy(ptr, settings_get_adc_offsets(), sizeof(int32_t) * NUM_ADC_CHANNELS);
    ptr += sizeof(int32_t) * NUM_ADC_CHANNELS;
    header_length += sizeof(int32_t) * NUM_ADC_CHANNELS;

    // Write the ADC channel labels
    for (uint8_t i = 0; i < NUM_ADC_CHANNELS; i++) {
        esp_err_t err = settings_get_ain_chan_label(i, channel_label);
        if (err != ESP_OK)
            return err;

        uint8_t label_len = strlen(channel_label);
        *ptr++ = label_len;
        memcpy(ptr, channel_label, label_len);
        ptr += label_len;
        header_length += sizeof(uint8_t) + label_len;
    }

    // Write the DIO channel labels
    for (uint8_t i = 0; i < NUM_DIO_CHANNELS; i++) {
        esp_err_t err = settings_get_dio_chan_label(i, channel_label);
        if (err != ESP_OK)
            return err;

        uint8_t label_len = strlen(channel_label);
        *ptr++ = label_len;
        memcpy(ptr, channel_label, label_len);
        ptr += label_len;
        header_length += sizeof(uint8_t) + label_len;
    }

    // Now go back to the start of the buffer and write the header length
    memcpy(header_buffer, &header_length, sizeof(uint32_t));

    // Write the entire header in one go
    int w = fwrite(header_buffer, 1, header_length + sizeof(uint32_t), f);
    if (w != header_length + sizeof(uint32_t))
    {
        ESP_LOGE(TAG_FILE, "Error writing full header to file. Expected %lu bytes, wrote %d bytes", header_length + sizeof(uint32_t), w);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_FILE, "Written full header size: %lu", header_length + sizeof(uint32_t));

    return ESP_OK;
}

