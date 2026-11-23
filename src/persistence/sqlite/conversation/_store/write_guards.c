/*
 * write_guards.c — C port of upstream Hermes
 * persistence/sqlite/conversation/_store/write_guards.py.
 * PoP: wguards_write_guards_reject @ persistence/sqlite/conversation/_store/write_guards.py:_write_guards_reject
 * PoP: wguards_guarded_ids @ persistence/sqlite/conversation/_store/write_guards.py:_guarded_ids
 */
#include "noc_wguards.h"
#include <stdlib.h>
#include <string.h>

/* _write_guards_reject returns True only for the two caught guard exceptions.
 * check_outcome mirrors what the (injected) _check_transcript_write_guards
 * does: 0=returns normally, 1=SessionCompressionInProgressError,
 * 2=SessionTurnLeaseLostError. Any other exception propagates in Python and
 * is therefore not represented as a False answer here. */
int wguards_write_guards_reject(int check_outcome) {
    return (check_outcome == 1 || check_outcome == 2) ? 1 : 0;
}

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

/* {sid for sid in ids if _write_guards_reject(...)} -> set -> caller sorts.
 * outcomes[i] is the injected guard-check outcome for ids[i]; repeated ids
 * guard the id if ANY occurrence is guarded. Returns count written to out[]. */
size_t wguards_guarded_ids(const char *ids[], const int *outcomes, size_t n,
                           const char *out[], size_t out_cap) {
    size_t m = 0;
    if (!ids || !outcomes || out_cap == 0) return 0;
    for (size_t i = 0; i < n; ++i) {
        if (!wguards_write_guards_reject(outcomes[i])) continue;
        int seen = 0;
        for (size_t j = 0; j < m; ++j) {
            if (!strcmp(out[j], ids[i])) { seen = 1; break; }
        }
        if (!seen && m < out_cap) out[m++] = ids[i];
    }
    qsort(out, m, sizeof(const char *), cmp_str);
    return m;
}
