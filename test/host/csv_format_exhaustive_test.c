// Exhaustive host test for the fast CSV formatter:
//  - csv_put_fixed() for EVERY int32 value, for all three column formats
//    (10V: /1e7 7 decimals, 60V: /1e6 6 decimals, NTC: /10 1 decimal),
//    compared with the original snprintf("%s%d%s%0Nd", ...) output;
//  - csv_put_uint() as used by the timestamp (widths 2 and 3) for every uint8
//    value and a sweep of uint32 values, compared with snprintf("%02d"/"%03lu").
//
// build & run:  gcc -O2 -pthread -o /tmp/csv_exh test/host/csv_format_exhaustive_test.c && /tmp/csv_exh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    uint8_t year, month, date, hours, minutes, seconds, padding1, padding2;
    uint32_t subseconds;
} s_date_time_t;

#include "../../main/csv_format.h"

typedef struct { int32_t factor; int width; const char *fmt; } column_t;

static const column_t COLUMNS[] = {
    { 10000000, 7, "%s%d%s%07d" },  // +/-10V
    { 1000000,  6, "%s%d%s%06d" },  // +/-60V
    { 10,       1, "%s%d%s%01d" },  // NTC temperature
};

typedef struct { int64_t lo, hi; int col; long mismatches; } job_t;

static void *worker(void *arg)
{
    job_t *j = arg;
    const column_t *c = &COLUMNS[j->col];
    char ref[48], got[48];
    for (int64_t v64 = j->lo; v64 < j->hi; v64++)
    {
        int32_t v = (int32_t)v64;
        int n = snprintf(ref, sizeof(ref), c->fmt, (v < 0) ? "-" : "", abs(v / c->factor), ",", abs(v % c->factor));
        char *e = csv_put_fixed(got, v, c->factor, c->width, ',');
        *e = '\0';
        if ((e - got) != n || memcmp(ref, got, n) != 0)
        {
            if (j->mismatches++ < 3)
                printf("MISMATCH col=%d v=%d ref=[%s] got=[%s]\n", j->col, v, ref, got);
        }
    }
    return NULL;
}

static long run_column(int col, int threads)
{
    pthread_t t[64];
    job_t jobs[64];
    int64_t lo = INT32_MIN, span = ((int64_t)INT32_MAX - INT32_MIN + 1) / threads;
    for (int i = 0; i < threads; i++)
    {
        jobs[i] = (job_t){ lo + i * span, (i == threads - 1) ? (int64_t)INT32_MAX + 1 : lo + (i + 1) * span, col, 0 };
        pthread_create(&t[i], NULL, worker, &jobs[i]);
    }
    long bad = 0;
    for (int i = 0; i < threads; i++)
    {
        pthread_join(t[i], NULL);
        bad += jobs[i].mismatches;
    }
    return bad;
}

static long check_uint_widths(void)
{
    char ref[24], got[24];
    long bad = 0, n = 0;
    // timestamp fields: uint8 with %02d
    for (int v = 0; v < 256; v++, n++)
    {
        snprintf(ref, sizeof(ref), "%02d", v);
        *csv_put_uint(got, (uint32_t)v, 2) = '\0';
        if (strcmp(ref, got)) { bad++; printf("MISMATCH %%02d v=%d ref=[%s] got=[%s]\n", v, ref, got); }
    }
    // subseconds: uint32 with %03lu; all values below 2^24 plus a stride over the rest
    for (uint64_t v = 0; v <= UINT32_MAX; v += (v < (1u << 24)) ? 1 : 65521, n++)
    {
        snprintf(ref, sizeof(ref), "%03lu", (unsigned long)v);
        *csv_put_uint(got, (uint32_t)v, 3) = '\0';
        if (strcmp(ref, got)) { if (bad++ < 3) printf("MISMATCH %%03lu v=%lu ref=[%s] got=[%s]\n", (unsigned long)v, ref, got); }
    }
    snprintf(ref, sizeof(ref), "%03lu", (unsigned long)UINT32_MAX);
    *csv_put_uint(got, UINT32_MAX, 3) = '\0';
    if (strcmp(ref, got)) bad++;
    printf("timestamp fields: %ld values checked, %ld mismatches\n", n + 1, bad);
    return bad;
}

int main(void)
{
    int threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (threads < 1) threads = 1;
    if (threads > 64) threads = 64;
    long bad = check_uint_widths();
    for (int c = 0; c < 3; c++)
    {
        long b = run_column(c, threads);
        printf("column format %-12s: all 4294967296 int32 values checked, %ld mismatches\n", COLUMNS[c].fmt, b);
        bad += b;
    }
    printf(bad ? "FAILED\n" : "PASSED\n");
    return bad != 0;
}
