#ifndef _LOGGER_H
#define _LOGGER_H
#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "settings.h"

// **********************************************************************************************
// DON'T TOUCH NEXT LINES PLEASE UNTIL NEXT ASTERIX LINES
//
#define DATA_LINES_PER_SPI_TRANSACTION  70
#define ADC_VALUES_PER_SPI_TRANSACTION  DATA_LINES_PER_SPI_TRANSACTION*8 // Number of ADC uint16_t per transaction.
#define ADC_BYTES_PER_SPI_TRANSACTION ADC_VALUES_PER_SPI_TRANSACTION*2
#define GPIO_BYTES_PER_SPI_TRANSACTION  DATA_LINES_PER_SPI_TRANSACTION
#define TIME_BYTES_PER_SPI_TRANSACTION  DATA_LINES_PER_SPI_TRANSACTION*12
#define START_STOP_NUM_BYTES            2

// the ADC value 0xFFFF is not possible, so we take that as start and stop bytes
#define START_STOP_BYTE_VALUE    0xFFFF

// Number of bytes when receiving data from the STM
#define STM_SPI_BUFFERSIZE_DATA_RX      (ADC_BYTES_PER_SPI_TRANSACTION + GPIO_BYTES_PER_SPI_TRANSACTION + TIME_BYTES_PER_SPI_TRANSACTION + START_STOP_NUM_BYTES)
#define STM_DATA_BUFFER_SIZE_PER_TRANSACTION (ADC_BYTES_PER_SPI_TRANSACTION + GPIO_BYTES_PER_SPI_TRANSACTION + TIME_BYTES_PER_SPI_TRANSACTION )

#define DATA_TRANSACTIONS_PER_SD_FLUSH 4
#define SD_BUFFERSIZE (DATA_TRANSACTIONS_PER_SD_FLUSH*STM_DATA_BUFFER_SIZE_PER_TRANSACTION) 

// It's essential to have the padding bytes in the right place manually, else the ADC DMA writes over the padding bytes

typedef struct {
    uint8_t startByte[START_STOP_NUM_BYTES]; // 2
    uint16_t dataLen;
    s_date_time_t timeData[DATA_LINES_PER_SPI_TRANSACTION]; //12*70 = 840
    uint8_t padding3;
    uint8_t padding4;
    uint8_t gpioData[GPIO_BYTES_PER_SPI_TRANSACTION]; // 70
    uint8_t adcData[ADC_BYTES_PER_SPI_TRANSACTION]; // 1120
} spi_msg_1_t ;

typedef struct {
    uint8_t adcData[ADC_BYTES_PER_SPI_TRANSACTION];
    uint8_t gpioData[GPIO_BYTES_PER_SPI_TRANSACTION];
    uint8_t padding1;
    uint8_t padding2;
    s_date_time_t timeData[DATA_LINES_PER_SPI_TRANSACTION];
    uint16_t dataLen;
    uint8_t stopByte[START_STOP_NUM_BYTES];
} spi_msg_2_t;
// END OF NO TOUCH
// *********************************************************************************************************************


typedef struct {
    uint8_t gpioData[6];
    float   temperatureData[8];
    float   analogData[8];
    uint64_t timestamp;
} converted_reading_t;

enum LogTaskStates
{
    LOGTASK_INIT = 0,
    LOGTASK_IDLE,
    LOGTASK_LOGGING,
    LOGTASK_SETTINGS,
    LOGTASK_STOPPING,
    LOGTASK_ERROR_OCCURED,
    LOGTASK_REBOOT_SYSTEM,
    LOGTASK_FWUPDATE,
    LOGTASK_SINGLE_SHOT,
    LOGTASK_CALIBRATION,
    LOGTASK_NUM_STATES
};


enum LoggingStates{
    LOGGING_IDLE = 0,
    LOGGING_WAIT_FOR_DATA_READY,
    LOGGING_RX0_WAIT,
    LOGGING_RX1_WAIT,
    LOGGING_GET_LAST_DATA,
    LOGGING_DONE,
    LOGGING_ERROR,
    LOGGING_NUM_STATES
};

enum Logger_modeButtonStates{
    MODEBUTTON_IDLE = 0,
    MODEBUTTON_HOLD,
    MODEBUTTON_RELEASED,
    MODEBUTTON_NUM_STATES
};



typedef uint8_t LoggerState_t;
typedef uint8_t LoggingState_t;


/// @brief Returns fixed point int32_t of ADC value
/// @param adcData0 LSB of ADC channel
/// @param adcData1 MSB of ADC channel
/// @param range Range of channel. I.e. if total range is 20V, use 20000000
/// @param offset Offset of the channel. If total range is 20V and minimum range value is -10V use 10000000
/// @return 
// int32_t Logger_convertAdcFixedPoint(uint8_t adcData0, uint8_t adcData1, uint64_t range, uint32_t offset);

esp_err_t Logger_calibrate();

float Logger_convertAdcFloat(uint16_t adcVal);

/**
 * @brief Enable or disable the interrupt for the data ready pin.
 *
 *
 * @note This function is non-blocking
 *
 * @param value 0 for disable, 1 for enable
 * @return
 *         - RET_NOK  if interrupt setting failed
 *         - RET_OK                on success
 */
// esp_err_t Logger_datardy_int(uint8_t value);

uint32_t Logger_getLastFreeSpace();
esp_err_t Logger_check_sdcard_free_space();
LoggerState_t Logger_getState();
uint32_t LogTaskGetError();
esp_err_t Logger_log();

esp_err_t LogTask_start();
esp_err_t LogTask_stop();

esp_err_t Logging_stop();
esp_err_t Logging_start();

esp_err_t Logger_singleShot();
uint32_t Logger_getError();


// uint8_t Logger_flush_buffer_to_sd_card();
size_t Logger_flush_buffer_to_sd_card_uint8(uint8_t * buffer, size_t size);
size_t Logger_flush_buffer_to_sd_card_csv(int32_t * adcData, size_t lenAdcBytes, uint8_t * gpioData, size_t lenGpio, uint8_t * timeData, size_t lenTime, size_t datarows);

uint8_t Logger_isLogging(void);

uint8_t Logger_setCsvLog(log_mode_t value);
uint8_t Logger_getCsvLog();

uint8_t Logger_mode_button();
esp_err_t Logger_syncSettings();

esp_err_t Logger_startFWupdate();

void Logging_restartSystem();


void Logger_GetSingleConversion(converted_reading_t* data);



#endif