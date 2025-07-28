/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__
/*
DEBUG OUTPUT CONFIGURATION
*/

// #define DEBUG_CONSOLE
//#define DEBUG_FILEMAN
// #define DEBUG_FILESERVER
// #define DEBUG_FIRMWARE_STM32
//#define DEBUG_FIRMWARE_WWW
#define DEBUG_LOGTASK
// #define DEBUG_LOGTASK_RX
// #define DEBUG_LOGGING
#define DEBUG_MAIN
// #define DEBUG_REST_SERVER
// #define DEBUG_SETTINGS
// #define DEBUG_SDCARD
// #define DEBUG_SPI_CONTROL
// #define DEBUG_WIFI

#define NUM_CALIBRATION_VALUES 10

/*
    HARDWARE CONFIGURATION
*/


/* STM32 */
#define GPIO_DATA_RDY_PIN 6
// #define GPIO_DATA_OVERRUN 10

// SPI STM32
#define STM32_SPI_MOSI 11
#define STM32_SPI_MISO 13
#define STM32_SPI_SCLK 12
#define STM32_SPI_CS 10
#define SPI_STM32_BUS_FREQUENCY 10000000U


#ifdef CONFIG_IDF_TARGET_ESP32
#define STM32_SPI_HOST HSPI_HOST

#else
#define STM32_SPI_HOST SPI3_HOST

#endif

// SPI SDCARD
#define SDCARD_SPI_MISO 37
#define SDCARD_SPI_MOSI 35
#define SDCARD_SPI_CLK  36
#define SDCARD_SPI_CS   34
#define SDCARD_CD       33
#define SDCARD_POWER_EN 2


// ADC enable
#define GPIO_ADC_EN 5
// External trigger pin value (= GPIO_STM32_UART_TX). Forwards the debounced external pin value
#define GPIO_EXT_PIN 3

// nBOOT0 and nRESET
#define GPIO_STM32_BOOT0 21
#define GPIO_STM32_NRESET 26

#define GPIO_STM32_UART_TX  3 
#define GPIO_STM32_UART_RX  4

/* HMI DISPLAY */
#define GPIO_OLED_SDA 41
#define GPIO_OLED_SCL 42

#define GPIO_HMI_LED_GREEN_HW_T00 38
#define GPIO_HMI_LED_RED_HW_T00 39
#define GPIO_START_STOP_BUTTON 0

#define GPIO_HMI_LED_GREEN_HW_T01 15 // LED1 in schematic
#define GPIO_HMI_LED_RED_HW_T01 14  // LED2 in schematic

#define BOARD_REV0 16
#define BOARD_REV1 17
#define BOARD_REV2 18



#define PROMPT_STR "uberlogger"

/* SD CARD */
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32H2
#define GPIO_HANDSHAKE 3
#define STM32_SPI_MOSI 7
#define STM32_SPI_MISO 2
#define STM32_SPI_SCLK 6
#define GPIO_CS 10

#elif CONFIG_IDF_TARGET_ESP32S3
#define GPIO_HANDSHAKE 2
#define STM32_SPI_MOSI 11
#define STM32_SPI_MISO 13
#define STM32_SPI_SCLK 12
#define GPIO_CS 10

#endif //CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2

#define SDCARD_FREE_SPACE_MINIMUM_KB    200
#define SDCARD_FREE_SPACE_WARNING_KB    1024*8
#define SDCARD_SPI_HOST SPI2_HOST

// This is for esp_sd_card.c
#define PIN_NUM_MISO SDCARD_SPI_MISO
#define PIN_NUM_MOSI SDCARD_SPI_MOSI
#define PIN_NUM_CLK  SDCARD_SPI_CLK
#define PIN_NUM_CS   SDCARD_SPI_CS
#define PIN_SD_CD    SDCARD_CD

      
// #define MAX_SDCARD_SIZE 4000000000
// #define MAX_FILE_SIZE MAX_SDCARD_SIZE //  1000000 // 100 MB in bytes
// #define MAX_FILE_SIZE 300*1024

#define V_OFFSET_60V 126774848
#define V_OFFSET_10V 151699029

/* File format settings */
#define RAW_FILE_FORMAT_VERSION 2

#endif