/*
 * cli_footer_split.c — real C11 port of upstream
 * hermes_cli/cli_footer_split.py.
 *
 * PoP: footersplit_divide_heights_overflow @ hermes_cli/cli_footer_split.py:FooterSplit._divide_heights
 *
 * The class' contribution is its overflow path: when the base HSplit refuses
 * to divide, allocate each row's minimum bottom-up against a shrinking budget
 * so a late-appearing row loses its height first. That deterministic path is
 * what this self-contained function ports.
 */
#include "noc_footersplit.h"
#include <stdlib.h>

/* PoP: footersplit_divide_heights_overflow @ hermes_cli/cli_footer_split.py:FooterSplit._divide_heights */
json_t *footersplit_divide_heights_overflow(long long budget, const json_t *mins) {
    json_t *out = json_array();
    size_t n, k;
    long long remaining;
    long long *clipped;
    if (!out) return NULL;
    n = json_is_array(mins) ? json_len(mins) : 0;
    clipped = (long long *)malloc((n ? n : 1) * sizeof(long long));
    if (!clipped) { json_free(out); return NULL; }
    remaining = (long long)budget;
    /* Python: for child in reversed(self._all_children):
     *   rows = min(child.preferred_height(width, budget).min, budget)
     *   budget -= rows; clipped.append(rows)
     * return clipped[::-1] */
    for (k = 0; k < n; k++) {
        size_t idx = n - 1 - k;
        long long m = (long long)json_number_value(json_get(mins, idx));
        long long rows = m < remaining ? m : remaining;
        remaining -= rows;
        clipped[idx] = rows;
    }
    for (k = 0; k < n; k++) json_append(out, json_number((double)clipped[k]));
    free(clipped);
    return out;
}

