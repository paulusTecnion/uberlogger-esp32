/*
 * Uberlogger Firmware
 * Copyright (c) 2025 Tecnion Technologies
 * Licensed under the MIT License.
 * See the README.md file in the project root for license details and hardware restrictions.
 */
#ifndef CSV_FORMAT_H
#define CSV_FORMAT_H

#include <stdint.h>
#include <stddef.h>

// Fast CSV row formatter used while logging. It replaces ~30 snprintf calls per
// row (too slow at 250 Hz) and produces byte-identical output to the former
// snprintf-based code in fileman_csv_write(). The caller must include the
// definition of s_date_time_t (settings.h) before this header.

// Longest possible row: timestamp (31) + 8 * (sep + '-' + 10 + dec + 10) +
// 6 * (sep + 1) + "\r\n" + '\0' < 240.
#define CSV_ROW_MAX_LEN 240

// An NTC value equal to this is written as an empty field (sensor open,
// shorted or out of range). Same value as NTC_TEMP_INVALID in tempsensor.h.
#define CSV_INVALID_VALUE INT32_MIN

typedef struct {
    uint8_t adc_enabled;   // bit x: analog channel x enabled
    uint8_t adc_ntc;       // bit x: analog channel x is an NTC (temperature)
    uint8_t adc_range60;   // bit x: analog channel x uses the 60V range
    uint8_t gpio_enabled;  // bit x: digital input x enabled
    char decimal;          // '.' or ','
    char separator;        // ',' or ';'
} csv_row_format_t;

// Write v in decimal, zero-padded to at least min_width digits (min_width <= 10).
static inline char *csv_put_uint(char *p, uint32_t v, int min_width)
{
    char tmp[10];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n < min_width)
        tmp[n++] = '0';
    while (n)
        *p++ = tmp[--n];
    return p;
}

// Same as "%s%d%s%0<frac_width>d" with ("-" if v < 0, abs(v / factor), dec, abs(v % factor)).
static inline char *csv_put_fixed(char *p, int32_t v, int32_t factor, int frac_width, char dec)
{
    int32_t q = v / factor;
    int32_t r = v % factor;
    if (v < 0)
        *p++ = '-';
    p = csv_put_uint(p, q < 0 ? (uint32_t)0 - (uint32_t)q : (uint32_t)q, 1);
    *p++ = dec;
    return csv_put_uint(p, r < 0 ? (uint32_t)0 - (uint32_t)r : (uint32_t)r, frac_width);
}

// Format one CSV row (NUL-terminated, ending in "\r\n") into out, which must hold
// CSV_ROW_MAX_LEN bytes. adc points at the 8 fixed-point values of this row.
// Returns the row length excluding the NUL.
static inline size_t csv_format_row(char *out, const csv_row_format_t *fmt,
                                    const int32_t *adc, uint8_t gpio,
                                    const s_date_time_t *t,
                                    int32_t factor_temp, int32_t factor_60v, int32_t factor_10v)
{
    char *p = out;

    // "20%02d-%02d-%02d %02d:%02d:%02d.%03lu"
    *p++ = '2';
    *p++ = '0';
    p = csv_put_uint(p, t->year, 2);
    *p++ = '-';
    p = csv_put_uint(p, t->month, 2);
    *p++ = '-';
    p = csv_put_uint(p, t->date, 2);
    *p++ = ' ';
    p = csv_put_uint(p, t->hours, 2);
    *p++ = ':';
    p = csv_put_uint(p, t->minutes, 2);
    *p++ = ':';
    p = csv_put_uint(p, t->seconds, 2);
    *p++ = '.';
    p = csv_put_uint(p, t->subseconds, 3);

    for (int x = 0; x < 8; x++)
    {
        if (!(fmt->adc_enabled & (1 << x)))
            continue;
        *p++ = fmt->separator;
        if (fmt->adc_ntc & (1 << x))
        {
            if (adc[x] != CSV_INVALID_VALUE)  // invalid: leave the field empty
                p = csv_put_fixed(p, adc[x], factor_temp, 1, fmt->decimal);  // 1 decimal for temperature
        }
        else if (fmt->adc_range60 & (1 << x))
            p = csv_put_fixed(p, adc[x], factor_60v, 6, fmt->decimal);   // 6 decimals for +/-60V
        else
            p = csv_put_fixed(p, adc[x], factor_10v, 7, fmt->decimal);   // 7 decimals for +/-10V
    }

    for (int x = 0; x < 6; x++)
    {
        if (!(fmt->gpio_enabled & (1 << x)))
            continue;
        *p++ = fmt->separator;
        *p++ = (gpio & (0x04 << x)) ? '1' : '0';
    }

    *p++ = '\r';
    *p++ = '\n';
    *p = '\0';
    return (size_t)(p - out);
}

#endif // CSV_FORMAT_H
