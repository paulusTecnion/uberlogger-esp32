#include "fileman.h"
#include "settings.h"
#include "config.h"

#define MOUNT_POINT "/sdcard"

static const char *TAG_FILE = "FILEMAN";
static uint32_t file_seq_num = 0;
static uint32_t file_seq_subnum = 0;
static FILE *f = NULL;
char file_name[50];
char filestrbuffer[4900];

uint32_t file_bytes_written = 0;

void fileman_create_filename()
{
    char filext[5];
    if (settings_get_logmode() == LOGMODE_CSV)
    {
        sprintf(filext, "%s", ".csv");
    }
    else
    {
        sprintf(filext, "%s", ".dat");
    }

    sprintf(file_name, MOUNT_POINT "/log%lu%s", file_seq_num, filext);
    // ESP_LOGI(TAG_FILE, "%s", file_name);
}

uint8_t fileman_check_current_file_size(size_t sizeInBytes)
{
    if (file_bytes_written > sizeInBytes)
    {
        ESP_LOGI(TAG_FILE, "File size exceeded %d bytes", sizeInBytes);
        file_bytes_written = 0;
        file_seq_subnum++;
        return 1;
    }
    else
    {
        return 0;
    }
}

void fileman_reset_subnum(void)
{
    file_seq_subnum = 0;
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
    f = fopen(file_name, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG_FILE, "Failed to open file for writing");
        return ESP_FAIL;
    }

    if (setvbuf(f, NULL, _IOFBF, 8192) != 0)
    {
        ESP_LOGE(TAG_FILE, "setvbuf failed");
        perror("The following error occurred");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t fileman_close_file(void)
{
    if (f != NULL)
    {
        fclose(f);
        f = NULL;
#ifdef DEBUG_FILEMAN
        ESP_LOGI(TAG_FILE, "Closing file");
#endif
        // file_seq_num++;
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG_FILE, "Cannot close file! f == NULL");
        return ESP_FAIL;
    }
}

esp_err_t fileman_search_last_sequence_file(void)
{
    f = NULL;
    file_seq_num = 0;

    while (1)
    {
        fileman_create_filename();
        f = fopen(file_name, "r");

        if (f == NULL)
        {
            break;
        }
        else
        {
            fclose(f);
        }

        file_seq_num++;
    }
#ifdef DEBUG_FILEMAN
    ESP_LOGI(TAG_FILE, "Sequence number: %d", file_seq_num);
#endif

    return file_seq_num;
}

int fileman_write(const void *data, size_t len)
{
    size_t write_result = fwrite(data, len, 1, f);
    if (write_result != 1)
    {
        ESP_LOGE(TAG_FILE, "Error writing to backing store %d %d\n", (int)len, write_result);
        perror("The following error occurred");
        return 0;
    }
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
        if ((settings_get()->adc_channel_type & (1 << i)))
        {
            writeptr = writeptr + sprintf(filestrbuffer + writeptr, "NTC%d,", i + 1);
        }
        else
        {
            writeptr = writeptr + sprintf(filestrbuffer + writeptr, "AIN%d,", i + 1);
        }
    }
    writeptr = writeptr + sprintf(filestrbuffer + writeptr, "DI1,DI2,DI3,DI4,DI5,DI6\r\n");
    // ESP_LOGI(TAG_FILE, "%s", filestrbuffer);
    // finally write to file and return
    return fprintf(f, filestrbuffer);
}

int fileman_csv_write_spi_msg(sdcard_data_t *sdcard_data, const int32_t *adcData)
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
                return ret;
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
    

    return ret;
}

int fileman_csv_write(const int32_t *dataAdc, const uint8_t *dataGpio,  const uint8_t *dataTime, size_t datarows)
{
    int j = 0;
    uint32_t writeptr = 0;

    s_date_time_t *date_time_ptr = (s_date_time_t *)dataTime;
    // ESP_LOGI(TAG_FILE,"Lengths: %u, %u, %u", lenAdc, lenGpio, lenTime);

    // for (int i = 0; i<32; i++)
    // {
    //     ESP_LOGI(TAG_FILE, "ADC fp: %ld", *(dataAdc+i*sizeof(int32_t)));
    // }

    // ESP_LOGI(TAG_FILE, "Writing %u rows", datarows);

    for (int i = 0; i < datarows; i++)
    {
        // Print time stamp
        writeptr = writeptr + sprintf(filestrbuffer + writeptr, "20%d-%02d-%02d %02d:%02d:%02d.%03lu,",
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

            // If temperature sensor and 16 bits we need to multiply input with 100 (or divide with factor 100 less which is ADC_MULT_FACTOR_16B_TEMP)
            if ((settings_get()->adc_channel_type & (1 << x)))
            {
                // Only 2 digits after decimal point for temperature
                writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d.%06d,",
                                               (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                               abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_16B_TEMP)),
                                               abs(dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_16B_TEMP));
            }
            else
            {
                // if range is 60V...
                if ((settings_get()->adc_channel_range & (1 << x)))
                {
                    // We need 6 digits after comma for +/-60V
                    writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d.%06d,",
                                                   (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_60V)),
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_60V));
                }
                else
                {
                    // We need 7 digits after comma for +/-10V
                    writeptr = writeptr + snprintf(filestrbuffer + writeptr, 14, "%s%d.%07d,",
                                                   (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] / (ADC_MULT_FACTOR_10V)),
                                                   abs(dataAdc[i * NUM_ADC_CHANNELS + x] % ADC_MULT_FACTOR_10V));
                }
            }
        }

        // Finally the IOs
        writeptr = writeptr + snprintf(filestrbuffer + writeptr, (2 * 6 + 5), "%d,%d,%d,%d,%d,%d\r\n",
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

        // int len = fprintf(f, (const char *)filestrbuffer);
        int len = fputs((const char*)filestrbuffer, f);
        // if (fputs((const char*)filestrbuffer, f) < 0)
        if (len < 0)
        {
            return 0;
        }
        file_bytes_written += len;
        writeptr = 0;
        //    }
    }

    return datarows;
}

// does this work?

// int fileman_csv_write(const int32_t *dataAdc, size_t lenAdc, const uint8_t* dataGpio, size_t lenGpio,
//                       const uint8_t* dataTime, size_t lenTime, size_t datarows) {
//     uint32_t writeptr = 0;
//     s_date_time_t *date_time_ptr = (s_date_time_t*)dataTime;
//     char buffer[BUFSIZE]; // Define BUFSIZE according to your maximum expected row size

//     for (size_t i = 0; i < datarows; i++, date_time_ptr++) {
//         // Format time stamp
//         writeptr += sprintf(buffer + writeptr, "20%d-%02d-%02d %02d:%02d:%02d.%03lu,",
//                             date_time_ptr->year, date_time_ptr->month, date_time_ptr->date,
//                             date_time_ptr->hours, date_time_ptr->minutes, date_time_ptr->seconds,
//                             date_time_ptr->subseconds);

//         // Format ADC
//         for (size_t x = 0; x < NUM_ADC_CHANNELS; x++) {
//             int32_t adcValue = dataAdc[i * NUM_ADC_CHANNELS + x];
//             int32_t factor = ((settings_get()->adc_channel_type & (1 << x)) ? ADC_MULT_FACTOR_16B_TEMP :
//                              (settings_get()->adc_channel_range & (1 << x)) ? ADC_MULT_FACTOR_60V : ADC_MULT_FACTOR_10V);

//             writeptr += snprintf(buffer + writeptr, 14, "%s%d.%0*d,",
//                                  (adcValue < 0) ? "-" : "", abs(adcValue / factor),
//                                  (factor == ADC_MULT_FACTOR_16B_TEMP) ? 2 : 6, abs(adcValue % factor));
//         }

//         // Format GPIO
//         uint8_t gpio = dataGpio[i];
//         writeptr += snprintf(buffer + writeptr, (2 * 6 + 5), "%d,%d,%d,%d,%d,%d\r\n",
//                              (gpio & 0x04) ? 1 : 0, (gpio & 0x08) ? 1 : 0,
//                              (gpio & 0x10) ? 1 : 0, (gpio & 0x20) ? 1 : 0,
//                              (gpio & 0x40) ? 1 : 0, (gpio & 0x80) ? 1 : 0);

//         // Write to file
//         if (f == NULL) return 0;
//         int len = fprintf(f, "%s", buffer);
//         if (len < 0) return 0;
//         file_bytes_written += len;
//         writeptr = 0;
//     }
//     return lenAdc;
// }

esp_err_t fileman_raw_write_header()
{
    int w = 0;
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
    // and finally  sample rate
    w = fwrite(&(settings_get()->log_sample_rate), sizeof(uint8_t), 1, f);
    if (w != sizeof(uint8_t))
        return ESP_FAIL;
    // and don't forget the calibration data
    w = fwrite(settings_get_adc_offsets(), sizeof(int32_t), NUM_ADC_CHANNELS, f);

    return ESP_OK;
}