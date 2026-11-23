#ifndef NOC_W6_DOWNLOADSTA_5C6FDC_H
#define NOC_W6_DOWNLOADSTA_5C6FDC_H
#include <stddef.h>
/* Partial keys that still have at least one file inside the grace window after
 * collect_partials() runs. Borrowed pointers into keys; sorted lexicographically. */
/* PoP: w6_downloadsta_5c6fdc_surviving_keys @ pm/download_state.py:collect_partials */
size_t w6_downloadsta_5c6fdc_surviving_keys(
    const char *const *keys, const int *has_part, const double *part_age,
    const int *has_ranges, const double *ranges_age, size_t n,
    double grace_seconds, const char **out, size_t cap);
#endif
