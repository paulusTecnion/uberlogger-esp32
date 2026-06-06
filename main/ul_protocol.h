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
 * each repo has a build-visible _Static_assert(UL_PROTOCOL_VERSION == N, ...)
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
#define UL_PROTOCOL_VERSION 2
_Static_assert(UL_PROTOCOL_VERSION == 2, "ul_protocol.h: unexpected UL_PROTOCOL_VERSION");

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
    STM32_CMD_GET_PROTOCOL_VERSION,   /* v2: ESP32 reads STM's UL_PROTOCOL_VERSION at boot */
    STM32_CMD_GET_OVERRUN,   /* v2: ESP32 reads STM's sticky ring-overrun flag (0/1) */
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

/* ===========================================================================
 * v2 STREAMING FRAME (UL_PROTOCOL_VERSION 2) — per-transaction base timestamp.
 *
 * One SPI transaction = one frame: a 14-byte header + a fixed-stride payload of
 *   adc[capacity * 8] (uint16) then gpio[capacity] (uint8).
 * Only the first `line_count` lines are valid; `capacity` is the per-session
 * stride (LINES_PER_FRAME). Block layout (adc block, then gpio block) keeps the
 * uint16 ADC block 2-byte aligned (M0+ friendly) and is what the ADC DMA
 * produces (sub-project C ready). Each line's time is reconstructed on the
 * ESP32 as t_i = base_epoch + base_subsec/65536 + i * period_us(fs_code).
 * ===========================================================================*/
#define UL_ADC_CH                 8
#define UL_FRAME_START0           0xFA
#define UL_FRAME_START1           0xFB

/* RAW .dat CONTAINER versioning. The existing .dat container is KEPT as-is (see
 * uberlogger-esp32 front/www/convert_raw.py — the inverse reader): a 4-byte
 * header_length, then 9 settings bytes where settings[0] = "File format version",
 * then 8 x int32 adc_offsets, then channel labels; frames follow; a trailing
 * uint64 total-row-count ends the file. The adc_offsets + labels + ranges in that
 * header are REQUIRED by the offline count->voltage conversion and are unchanged.
 * v2 bumps ONLY settings[0] from 2 -> 3 to signal the new frame body inside;
 * convert_raw.py detects 2 (old spi_msg_1/2 frames) vs 3 (new v2 frames). No new
 * magic is introduced. (The per-frame start markers + protocol_version below are
 * the SPI-stream resync/guard, independent of the file container.) */
#define UL_RAW_FILE_FORMAT_VERSION   3

/* flags byte */
#define UL_FLAG_RES16             (1u << 0)     /* 0 = 12-bit, 1 = 16-bit */
/* bits 1..7 reserved for sub-projects B/C (raw-ADC/DMA-direct, triggered) */

/* v2 frame header — exactly 14 bytes on the wire (packed; uint32 base_epoch
 * would otherwise force a 4-byte struct alignment and pad sizeof to 16). The
 * `pad` field keeps the 13 meaningful bytes + 1 reserved byte = 14. */
typedef struct __attribute__((packed)) {
    uint8_t  start[2];          /* UL_FRAME_START0, UL_FRAME_START1 */
    uint8_t  protocol_version;  /* = UL_PROTOCOL_VERSION (2) */
    uint8_t  flags;             /* UL_FLAG_RES16 | reserved */
    uint32_t base_epoch;        /* unix seconds at line 0 (RTC, once per frame) */
    uint16_t base_subsec;       /* Q16 fractional second (units of 1/65536 s) */
    uint8_t  fs_code;           /* adc_sample_rate_e index -> ul_period_us() */
    uint8_t  line_count;        /* valid lines in this frame (<= capacity) */
    uint8_t  capacity;          /* per-session stride = LINES_PER_FRAME */
    uint8_t  pad;               /* alignment / reserved */
} ul_frame_hdr_t;

_Static_assert(sizeof(ul_frame_hdr_t) == 14, "ul_frame_hdr_t must be 14 bytes on the wire");
_Static_assert(offsetof(ul_frame_hdr_t, base_epoch) == 4,  "ul_frame_hdr_t.base_epoch moved");
_Static_assert(offsetof(ul_frame_hdr_t, base_subsec) == 8, "ul_frame_hdr_t.base_subsec moved");
_Static_assert(offsetof(ul_frame_hdr_t, fs_code) == 10,    "ul_frame_hdr_t.fs_code moved");

/* Per-line wire size: 8 ADC uint16 (16 B, in the adc block) + 1 GPIO byte. */
#define UL_LINE_BYTES   (UL_ADC_CH * 2 + 1)   /* = 17 */

/* fs_code (== adc_sample_rate_e index) -> per-line period in microseconds.
 * 0 = sub-1Hz averaging path / unknown (no fixed period). Shared by both repos
 * so the ESP32 reconstruction and the STM >250Hz guard agree. */
static inline uint32_t ul_period_us(uint8_t fs_code) {
    switch (fs_code) {
        case 5:  return 1000000u; /* 1 Hz   */
        case 6:  return 500000u;  /* 2 Hz   */
        case 7:  return 200000u;  /* 5 Hz   */
        case 8:  return 100000u;  /* 10 Hz  */
        case 9:  return 40000u;   /* 25 Hz  */
        case 10: return 20000u;   /* 50 Hz  */
        case 11: return 10000u;   /* 100 Hz */
        case 12: return 4000u;    /* 250 Hz */
        case 13: return 2000u;    /* 500 Hz */
        case 14: return 1000u;    /* 1000 Hz*/
        default: return 0u;       /* averaging / unknown */
    }
}

/* True for rates that are 12-bit-RAW-only (CSV + 16-bit disabled). */
static inline int ul_is_high_rate(uint8_t fs_code) { return fs_code >= 13; }

#endif /* UL_PROTOCOL_H */
