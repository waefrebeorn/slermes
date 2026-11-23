/*
 * download_state.c - Resumable-partial GC boundary (C11 port).
 *
 * PoP: w6_downloadsta_5c6fdc_surviving_keys @ pm/download_state.py:collect_partials
 *
 * LIVE collect_partials() lists .part/.ranges stems, acquires each lock
 * non-blocking, and deletes a pair unless a member exists and was touched
 * within grace_seconds. The fixture pre-resolves the file set and ages; this
 * function reproduces the keep/delete decision and returns the surviving keys.
 */
#include "noc_w6_downloadsta_5c6fdc.h"

#include <stdlib.h>
#include <string.h>

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x, y);
}
size_t w6_downloadsta_5c6fdc_surviving_keys(
    const char *const *keys, const int *has_part, const double *part_age,
    const int *has_ranges, const double *ranges_age, size_t n,
    double grace_seconds, const char **out, size_t cap) {
    size_t k = 0;
    for (size_t i = 0; i < n && k < cap; i++) {
        int keep = 0;
        if (grace_seconds > 0) {
            if (has_part[i] && part_age[i] < grace_seconds) keep = 1;
            if (has_ranges[i] && ranges_age[i] < grace_seconds) keep = 1;
        }
        if (keep) out[k++] = keys[i];
    }
    qsort(out, k, sizeof(const char *), cmp_str);
    return k;
}

