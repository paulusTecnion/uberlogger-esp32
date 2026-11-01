// Host test: csv_format_row() must be byte-identical to the original
// snprintf-based row formatting from fileman_csv_write().
//
// build & run:  gcc -O2 -Wall -o /tmp/csv_format_test test/host/csv_format_test.c && /tmp/csv_format_test
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

typedef struct {
    uint8_t year, month, date, hours, minutes, seconds, padding1, padding2;
    uint32_t subseconds;
} s_date_time_t;

#include "../../main/csv_format.h"

#define NUM_ADC_CHANNELS 8
#define F_TEMP 10
#define F_60V 1000000
#define F_10V 10000000

// Reference: the original fileman_csv_write() body for one row (verbatim apart
// from settings_get_*() replaced by the same masks).
static int ref_row(char *filestrbuffer, size_t buffer_size, const csv_row_format_t *fmt,
                   const int32_t *dataAdc, const uint8_t *dataGpio, const s_date_time_t *date_time_ptr)
{
    int i = 0, j = 0;
    uint32_t writeptr = 0;
    const char *decimal_char = (fmt->decimal == ',') ? "," : ".";
    const char *separator_char = (fmt->separator == ',') ? "," : ";";
    bool first_field = true;

    int ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr,
                       "20%02d-%02d-%02d %02d:%02d:%02d.%03lu",
                       date_time_ptr[j].year, date_time_ptr[j].month, date_time_ptr[j].date,
                       date_time_ptr[j].hours, date_time_ptr[j].minutes, date_time_ptr[j].seconds,
                       (unsigned long)date_time_ptr[j].subseconds);
    writeptr += ret;
    first_field = false;

    for (int x = 0; x < NUM_ADC_CHANNELS; x++)
    {
        if (!(fmt->adc_enabled & (1 << x)))
            continue;
        if (!first_field) {
            ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr, "%s", separator_char);
            writeptr += ret;
        }
        if ((fmt->adc_ntc & (1 << x)))
        {
            ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr, "%s%d%s%01d",
                           (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                           abs(dataAdc[i * NUM_ADC_CHANNELS + x] / F_TEMP),
                           decimal_char,
                           abs(dataAdc[i * NUM_ADC_CHANNELS + x] % F_TEMP));
        }
        else if ((fmt->adc_range60 & (1 << x)))
        {
            ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr, "%s%d%s%06d",
                           (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                           abs(dataAdc[i * NUM_ADC_CHANNELS + x] / F_60V),
                           decimal_char,
                           abs(dataAdc[i * NUM_ADC_CHANNELS + x] % F_60V));
        }
        else
        {
            ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr, "%s%d%s%07d",
                           (dataAdc[i * NUM_ADC_CHANNELS + x] < 0) ? "-" : "",
                           abs(dataAdc[i * NUM_ADC_CHANNELS + x] / F_10V),
                           decimal_char,
                           abs(dataAdc[i * NUM_ADC_CHANNELS + x] % F_10V));
        }
        writeptr += ret;
        first_field = false;
    }

    for (int x = 0; x < 6; x++)
    {
        if (!(fmt->gpio_enabled & (1 << x)))
            continue;
        if (!first_field) {
            ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr, "%s", separator_char);
            writeptr += ret;
        }
        ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr, "%d",
                       (dataGpio[j] & (0x04 << x)) ? 1 : 0);
        writeptr += ret;
        first_field = false;
    }

    ret = snprintf(filestrbuffer + writeptr, buffer_size - writeptr, "\r\n");
    writeptr += ret;
    return (int)writeptr;
}

static uint64_t rng = 0x9E3779B97F4A7C15ULL;
static uint32_t rnd(void)
{
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return (uint32_t)rng;
}

static int32_t pick_value(void)
{
    static const int32_t edges[] = {
        0, 1, -1, 9, -9, 10, -10, 11, -11, 999999, -999999, 1000000, -1000000, 1000001, -1000001,
        9999999, -9999999, 10000000, -10000000, 10000001, -10000001,
        126774848, -126774848, 151699029, -151699029, INT32_MAX, INT32_MIN, INT32_MIN + 1,
    };
    uint32_t k = rnd() % 4;
    if (k == 0)
        return edges[rnd() % (sizeof(edges) / sizeof(edges[0]))];
    if (k == 1)
        return (int32_t)(rnd() % 2000001) - 1000000;      // small, around 0
    if (k == 2)
        return (int32_t)(rnd() % 400000001) - 200000000;  // full voltage range
    return (int32_t)rnd();                                 // anything
}

int main(void)
{
    char a[512], b[CSV_ROW_MAX_LEN];
    long rows = 0, max_len = 0;

    for (long n = 0; n < 3000000; n++)
    {
        csv_row_format_t fmt = {
            .adc_enabled = (uint8_t)rnd(), .adc_ntc = (uint8_t)rnd(), .adc_range60 = (uint8_t)rnd(),
            .gpio_enabled = (uint8_t)(rnd() & 0x3F),
            .decimal = (rnd() & 1) ? ',' : '.', .separator = (rnd() & 1) ? ',' : ';',
        };
        if (n % 3 == 0) { fmt.adc_enabled = 0xFF; fmt.gpio_enabled = 0x3F; }

        int32_t adc[8];
        for (int x = 0; x < 8; x++)
            adc[x] = pick_value();
        uint8_t gpio = (uint8_t)rnd();
        s_date_time_t t = {
            .year = (uint8_t)rnd(), .month = (uint8_t)rnd(), .date = (uint8_t)rnd(),
            .hours = (uint8_t)rnd(), .minutes = (uint8_t)rnd(), .seconds = (uint8_t)rnd(),
            .subseconds = (rnd() & 1) ? rnd() % 1000 : rnd(),
        };

        int la = ref_row(a, sizeof(a), &fmt, adc, &gpio, &t);
        size_t lb = csv_format_row(b, &fmt, adc, gpio, &t, F_TEMP, F_60V, F_10V);
        if ((size_t)la != lb || strcmp(a, b) != 0)
        {
            printf("MISMATCH at row %ld\n  ref: [%s]\n  new: [%s]\n", n, a, b);
            return 1;
        }
        if (la > max_len)
            max_len = la;
        rows++;
    }
    printf("OK: %ld rows identical, longest row %ld bytes (CSV_ROW_MAX_LEN %d)\n", rows, max_len, CSV_ROW_MAX_LEN);
    return 0;
}
