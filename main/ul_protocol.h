/*
 * MIT License
 *
 * Copyright (c) 2025 Tecnion Technologies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * ul_protocol.h
 *
 * Uberlogger STM32 <-> ESP32 SPI protocol: the SINGLE SOURCE OF TRUTH.
 *
 * This file is VENDORED BYTE-IDENTICALLY into both repositories:
 *   - uberlogger-stm32 : Core/Inc/ul_protocol.h
 *   - uberlogger-esp32 : main/ul_protocol.h
 * The two copies MUST be diff-clean. UL_PROTOCOL_VERSION is the sync tripwire:
 * each repo has a build-visible _Static_assert(UL_PROTOCOL_VERSION == 1, ...)
 * so a stale copy fails the build. Prose companion: see
 * uberlogger-stm32/docs/protocol/uberlogger-spi-protocol.md.
 *
 * BYTE-IDENTICAL CONTRACT: changing any name's *value*, any field order, any
 * padding, or any layout #define here changes the SPI wire format. Do not.
 *
 * --- spi_cmd_t naming reconciliation (Phase 1, Task 9) ---------------------
 * Historically the two repos disagreed on type NAMES (not bytes):
 *
 *   STM32 (esp32_interface.h)        ESP32 (spi_control.h)
 *   ------------------------         ---------------------
 *   command enum : spi_cmd_esp_t     command enum : stm32cmd_t
 *   8-byte struct: spi_cmd_t         8-byte struct: spi_cmd_t   <- already agreed
 *   2nd byte name: .data             2nd byte name: .data0      <- only divergence
 *
 * Resolution (TYPE-NAME / FIELD-NAME only; zero byte/value/logic change):
 *   - The command ENUM is named `stm32cmd_t` (ESP32 convention). A typedef
 *     alias `spi_cmd_esp_t` is provided so STM's historical name still resolves.
 *   - The 8-byte command STRUCT is named `spi_cmd_t` (both repos already agreed).
 *   - The struct's 2nd byte is exposed under BOTH names `data` (STM call sites)
 *     and `data0` (ESP32 call sites) via an anonymous union, so neither repo's
 *     call sites need editing. Same offset, same byte.
 *   - All response constants from both repos are kept (same values 1/2), so no
 *     call site breaks: STM32_RESP_OK/NOK, RESP_OK/NOK, spi_cmd_resp_t.
 * ---------------------------------------------------------------------------
 */

#ifndef UL_PROTOCOL_H
#define UL_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* Protocol contract version. Bump ONLY with an intentional wire change.
 * Both vendored copies must carry the same value; each repo asserts it. */
#define UL_PROTOCOL_VERSION 1
_Static_assert(UL_PROTOCOL_VERSION == 1, "ul_protocol.h: unexpected UL_PROTOCOL_VERSION");

/* ===========================================================================
 * Command set (ESP32 -> STM32 command path)
 * ===========================================================================*/

/* Command enum. Canonical name: stm32cmd_t (ESP32 convention). */
typedef enum stm32cmd {
    STM32_CMD_NOP = 0x00,
    STM32_CMD_SETTINGS_MODE,
    STM32_CMD_SETTINGS_SYNC,
    STM32_CMD_MEASURE_MODE,
    STM32_CMD_SET_RESOLUTION,
    STM32_CMD_SET_SAMPLE_RATE,
    STM32_CMD_SET_ADC_CHANNELS_ENABLED,
    STM32_CMD_SET_DATETIME,
    STM32_CMD_SINGLE_SHOT_MEASUREMENT,
    STM32_CMD_SEND_LAST_ADC_BYTES,
    STM32_CMD_SET_LOGMODE,
    STM32_CMD_SET_RANGE,
    STM32_CMD_SET_TRIGGER_MODE,
    CMD_UNKNOWN
} stm32cmd_t;

/* Historical STM32 alias for the command enum (see reconciliation note). */
typedef stm32cmd_t spi_cmd_esp_t;

/* 8-byte command/response struct. Canonical name: spi_cmd_t.
 * The 2nd byte carries the primary argument; STM call sites name it `data`,
 * ESP32 call sites name it `data0`. Anonymous union exposes both names at the
 * same offset. Layout/byte semantics are identical to both repos' originals. */
typedef struct {
    uint8_t command;
    union {
        uint8_t data;   /* STM32 call-site name  */
        uint8_t data0;  /* ESP32 call-site name  */
    };
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;
    uint8_t data4;
    uint8_t data5;
    uint8_t data6;
} spi_cmd_t;

_Static_assert(sizeof(spi_cmd_t) == 8, "spi_cmd_t must be exactly 8 bytes on the wire");

/* ===========================================================================
 * Response codes (STM32 -> ESP32). All values are wire-stable (OK=1, NOK=2).
 * ===========================================================================*/

/* ESP32 response enum (checked as spi_buffer[1] == STM32_RESP_OK). */
typedef enum stm32resp {
    STM32_RESP_OK = 1,
    STM32_RESP_NOK
} stm32resp_t;

/* STM32 historical anonymous response enum (same values). */
enum {
    RESP_OK = 1,
    RESP_NOK
};

/* STM32 historical response enum used as the command-response byte (resp.data). */
typedef enum {
    CMD_RESP_NOP = 0,
    CMD_RESP_OK,
    CMD_RESP_NOK
} spi_cmd_resp_t;

/* ===========================================================================
 * Streaming data path: SPI transaction layout
 *
 * WARNING: DO NOT CHANGE THESE NUMBERS UNLESS YOU KNOW WHAT YOU ARE DOING.
 * BYTES NEED TO BE 4 BYTES ALIGNED!
 * ===========================================================================*/
#define DATA_LINES_PER_SPI_TRANSACTION  70
#define ADC_VALUES_PER_SPI_TRANSACTION  DATA_LINES_PER_SPI_TRANSACTION*8 // Number of ADC uint16_t per transaction. This is 5 times 480 ADC values
#define ADC_BYTES_PER_SPI_TRANSACTION ADC_VALUES_PER_SPI_TRANSACTION*2
#define GPIO_BYTES_PER_SPI_TRANSACTION  DATA_LINES_PER_SPI_TRANSACTION*1
#define TIME_BYTES_PER_SPI_TRANSACTION  DATA_LINES_PER_SPI_TRANSACTION*12
#define START_STOP_NUM_BYTES            2

/* Date/time stamp carried per data line (12 bytes on the wire). */
typedef struct {
	uint8_t year;
	uint8_t month;
	uint8_t date;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint8_t padding1;
	uint8_t padding2;
	uint32_t subseconds;
} s_date_time_t;

/* Streaming message variant 1 (start-byte framed). */
typedef struct {
    uint8_t startByte[START_STOP_NUM_BYTES]; // 2
    uint16_t dataLen;
    uint8_t padding0[12];
    s_date_time_t timeData[DATA_LINES_PER_SPI_TRANSACTION]; //12*70 = 840
    uint8_t gpioData[GPIO_BYTES_PER_SPI_TRANSACTION]; // 70
    uint8_t padding1[2];
    union
    {
        uint8_t adcData[ADC_BYTES_PER_SPI_TRANSACTION]; // 1120
        uint16_t adcData16[ADC_VALUES_PER_SPI_TRANSACTION];
    };

} spi_msg_1_t;

/* Streaming message variant 2 (stop-byte framed, mirror layout). */
typedef struct {
    union {
        uint8_t adcData[ADC_BYTES_PER_SPI_TRANSACTION];
        uint16_t adcData16[ADC_VALUES_PER_SPI_TRANSACTION];
    };
    uint8_t padding1[2];
    uint8_t gpioData[GPIO_BYTES_PER_SPI_TRANSACTION];
    s_date_time_t timeData[DATA_LINES_PER_SPI_TRANSACTION];
    uint8_t padding0[12];
    uint16_t dataLen;
    uint8_t stopByte[START_STOP_NUM_BYTES];
} spi_msg_2_t;

/* --- Phase 1 wire-layout pins (refactor spec sec. 8). DO NOT change these numbers.
 * If a build fails here, a struct layout changed = the SPI wire format changed. --- */
_Static_assert(sizeof(s_date_time_t) == 12, "s_date_time_t layout changed");

_Static_assert(sizeof(spi_msg_1_t) == 2048, "spi_msg_1_t size changed");
_Static_assert(offsetof(spi_msg_1_t, timeData) == 16,  "spi_msg_1_t.timeData moved");
_Static_assert(offsetof(spi_msg_1_t, gpioData) == 856, "spi_msg_1_t.gpioData moved");
_Static_assert(offsetof(spi_msg_1_t, padding1) == 926, "spi_msg_1_t.padding1 moved");
_Static_assert(offsetof(spi_msg_1_t, adcData)  == 928, "spi_msg_1_t.adcData moved");

_Static_assert(sizeof(spi_msg_2_t) == 2048, "spi_msg_2_t size changed");
_Static_assert(offsetof(spi_msg_2_t, adcData)  == 0,    "spi_msg_2_t.adcData moved");
_Static_assert(offsetof(spi_msg_2_t, gpioData) == 1122, "spi_msg_2_t.gpioData moved");
_Static_assert(offsetof(spi_msg_2_t, timeData) == 1192, "spi_msg_2_t.timeData moved");
_Static_assert(offsetof(spi_msg_2_t, padding0) == 2032, "spi_msg_2_t.padding0 moved");
_Static_assert(offsetof(spi_msg_2_t, dataLen)  == 2044, "spi_msg_2_t.dataLen moved");
_Static_assert(offsetof(spi_msg_2_t, stopByte) == 2046, "spi_msg_2_t.stopByte moved");

#endif /* UL_PROTOCOL_H */
