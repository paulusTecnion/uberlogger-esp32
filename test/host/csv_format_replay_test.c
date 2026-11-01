// Replay test: feed CSV files written ON THE DEVICE by the old snprintf-based
// firmware back through the new formatter and require byte-identical lines.
//
// Each line is parsed back into the values the firmware had (timestamp fields,
// fixed-point ADC integers, GPIO bits), formatted again with csv_format_row(),
// and compared with the original line including "\r\n".
//
// build & run:  gcc -O2 -o /tmp/csv_replay test/host/csv_format_replay_test.c && /tmp/csv_replay file1.csv [file2.csv ...]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    uint8_t year, month, date, hours, minutes, seconds, padding1, padding2;
    uint32_t subseconds;
} s_date_time_t;

#include "../../main/csv_format.h"

#define F_TEMP 10
#define F_60V 1000000
#define F_10V 10000000

static int parse_uint(const char **p, uint32_t *out)
{
    const char *s = *p;
    uint64_t v = 0;
    if (*s < '0' || *s > '9')
        return -1;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (uint64_t)(*s++ - '0');
    *out = (uint32_t)v;
    *p = s;
    return 0;
}

// Parse "[-]int.frac" into a fixed-point integer; returns number of decimals or -1.
static int parse_fixed(const char **p, char dec, int32_t *out)
{
    const char *s = *p;
    int neg = 0;
    uint32_t ip = 0, fp = 0;
    if (*s == '-') { neg = 1; s++; }
    if (parse_uint(&s, &ip)) return -1;
    if (*s != dec) return -1;
    s++;
    const char *f0 = s;
    if (parse_uint(&s, &fp)) return -1;
    int ndec = (int)(s - f0);
    int64_t factor = ndec == 7 ? F_10V : ndec == 6 ? F_60V : ndec == 1 ? F_TEMP : 0;
    if (!factor) return -1;
    int64_t v = (int64_t)ip * factor + fp;
    *out = (int32_t)(neg ? -v : v);
    *p = s;
    return ndec;
}

static int replay_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror(path); return 1; }
    static char line[1024], out[CSV_ROW_MAX_LEN];
    long rows = 0, bad = 0, lineno = 0;
    int adc_cols = 0, gpio_cols = 0;
    const char sep = ',', dec = '.';

    while (fgets(line, sizeof(line), fp))
    {
        lineno++;
        if (strncmp(line, "20", 2) != 0)
        {
            // header: count AIN / DIO columns
            if (strstr(line, "AIN") || strstr(line, "DIO"))
            {
                adc_cols = gpio_cols = 0;
                for (char *t = line; (t = strchr(t, sep)); t++)
                {
                    if (!strncmp(t + 1, "DIO", 3)) gpio_cols++;
                    else adc_cols++;
                }
            }
            continue;
        }

        // timestamp "20YY-MM-DD hh:mm:ss.sss"
        const char *p = line + 2;
        uint32_t f[7];
        const char seps[6] = { '-', '-', ' ', ':', ':', '.' };
        int ok = 1;
        for (int i = 0; i < 7 && ok; i++)
        {
            ok = parse_uint(&p, &f[i]) == 0;
            if (ok && i < 6) ok = (*p++ == seps[i]);
        }
        s_date_time_t t = { (uint8_t)f[0], (uint8_t)f[1], (uint8_t)f[2], (uint8_t)f[3], (uint8_t)f[4], (uint8_t)f[5], 0, 0, f[6] };

        // Channel layout: assume the first adc_cols analog channels / gpio_cols digital inputs are enabled.
        csv_row_format_t fmt = { (uint8_t)((1u << adc_cols) - 1), 0, 0, (uint8_t)((1u << gpio_cols) - 1), dec, sep };
        int32_t adc[8] = { 0 };
        uint8_t gpio = 0;
        for (int x = 0; x < adc_cols && ok; x++)
        {
            ok = (*p++ == sep);
            int nd = ok ? parse_fixed(&p, dec, &adc[x]) : -1;
            if (nd == 1) fmt.adc_ntc |= (uint8_t)(1 << x);
            else if (nd == 6) fmt.adc_range60 |= (uint8_t)(1 << x);
            ok = ok && nd > 0;
        }
        for (int x = 0; x < gpio_cols && ok; x++)
        {
            ok = (*p++ == sep) && (*p == '0' || *p == '1');
            if (ok && *p++ == '1') gpio |= (uint8_t)(0x04 << x);
        }
        ok = ok && !strcmp(p, "\r\n");

        size_t n = ok ? csv_format_row(out, &fmt, adc, gpio, &t, F_TEMP, F_60V, F_10V) : 0;
        if (!ok || strcmp(out, line) != 0)
        {
            if (bad++ < 5)
                printf("  MISMATCH line %ld%s\n    file: %s    new:  %s", lineno, ok ? "" : " (unparseable)", line, ok ? out : "-\n");
        }
        (void)n;
        rows++;
    }
    fclose(fp);
    printf("%s: %ld rows (%d analog, %d digital columns), %ld mismatches\n", path, rows, adc_cols, gpio_cols, bad);
    return bad != 0;
}

int main(int argc, char **argv)
{
    int fail = 0;
    for (int i = 1; i < argc; i++)
        fail |= replay_file(argv[i]);
    printf(fail ? "FAILED\n" : "PASSED\n");
    return fail;
}
