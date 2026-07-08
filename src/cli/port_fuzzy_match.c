/*
 * port_fuzzy_match.c
 *
 * Faithful C11 port of the PURE helper from tools/fuzzy_match.py:
 *   _map_normalized_positions  ->  fz_map_normalized_positions
 *
 * Maps match positions found in a *normalized* string back to the
 * corresponding positions in the *original* string (best-effort, correct for
 * whitespace normalization: spaces/tabs collapsed to single spaces, extra
 * whitespace dropped). Pure string index arithmetic — no I/O, no regex,
 * no external state. Carries its PoP annotation.
 *
 * Interface (C adaptation of the Python signature):
 *   Caller passes the original/normalized strings and the normalized matches
 *   as an array of (start,end) int pairs. The function fills `out` (same
 *   shape, capacity >= nmatch rows) and returns the number of mapped rows
 *   (= nmatch, or 0 if nmatch == 0).
 */

#include <string.h>
#include <stdlib.h>

/* PoP: fz_map_normalized_positions @ tools/fuzzy_match.py:_map_normalized_positions */
int fz_map_normalized_positions(const char *original,
                               const char *normalized,
                               const int norm_matches[][2], int nmatch,
                               int out[][2])
{
    if (nmatch <= 0) return 0;

    size_t olen = strlen(original);
    size_t nlen = strlen(normalized);

    /* --- build orig_to_norm[]: for each original position, the matching
     *     normalized index (or the final normalized length for trailing) --- */
    int *orig_to_norm = (int *)malloc((olen + 1) * sizeof(int));
    if (!orig_to_norm) return 0;

    size_t orig_idx = 0, norm_idx = 0;
    while (orig_idx < olen && norm_idx < nlen) {
        if (original[orig_idx] == normalized[norm_idx]) {
            orig_to_norm[orig_idx] = (int)norm_idx;
            orig_idx++;
            norm_idx++;
        } else if ((original[orig_idx] == ' ' || original[orig_idx] == '\t') &&
                   normalized[norm_idx] == ' ') {
            /* original space/tab collapsed to a single normalized space */
            orig_to_norm[orig_idx] = (int)norm_idx;
            orig_idx++;
            /* don't advance norm_idx yet — wait until whitespace consumed */
            if (orig_idx < olen &&
                original[orig_idx] != ' ' && original[orig_idx] != '\t')
                norm_idx++;
        } else if (original[orig_idx] == ' ' || original[orig_idx] == '\t') {
            /* extra whitespace in original, dropped in normalized */
            orig_to_norm[orig_idx] = (int)norm_idx;
            orig_idx++;
        } else {
            /* mismatch (shouldn't happen with our normalization) */
            orig_to_norm[orig_idx] = (int)norm_idx;
            orig_idx++;
        }
    }
    while (orig_idx < olen) {
        orig_to_norm[orig_idx] = (int)nlen;
        orig_idx++;
    }

    /* --- reverse mapping: normalized index -> original start/end --- */
    /* We use two parallel arrays indexed by normalized position. */
    int *norm_to_orig_start = (int *)malloc((nlen + 1) * sizeof(int));
    int *norm_to_orig_end   = (int *)malloc((nlen + 1) * sizeof(int));
    if (!norm_to_orig_start || !norm_to_orig_end) {
        free(orig_to_norm);
        free(norm_to_orig_start);
        free(norm_to_orig_end);
        return 0;
    }
    for (size_t i = 0; i <= nlen; i++) {
        norm_to_orig_start[i] = -1;
        norm_to_orig_end[i]   = -1;
    }
    for (size_t op = 0; op < olen; op++) {
        int np = orig_to_norm[op];
        if (norm_to_orig_start[np] == -1)
            norm_to_orig_start[np] = (int)op;
        norm_to_orig_end[np] = (int)op;
    }

    /* --- map each normalized match back to original coordinates --- */
    for (int m = 0; m < nmatch; m++) {
        int norm_start = norm_matches[m][0];
        int norm_end   = norm_matches[m][1];

        int orig_start;
        if (norm_start <= (int)nlen && norm_to_orig_start[norm_start] != -1)
            orig_start = norm_to_orig_start[norm_start];
        else {
            /* find nearest original position whose normalized index >= norm_start */
            int best = (int)olen;
            for (size_t op = 0; op < olen; op++)
                if (orig_to_norm[op] >= norm_start) { best = (int)op; break; }
            orig_start = best;
        }

        int orig_end;
        int key = norm_end - 1;
        if (key >= 0 && key <= (int)nlen && norm_to_orig_end[key] != -1)
            orig_end = norm_to_orig_end[key] + 1;
        else
            orig_end = orig_start + (norm_end - norm_start);

        /* Expand to include trailing whitespace that was normalized, but only
         * when the normalized match itself ended with whitespace. */
        if (norm_end < (int)nlen && normalized[norm_end - 1] == ' ') {
            while (orig_end < (int)olen &&
                   (original[orig_end] == ' ' || original[orig_end] == '\t'))
                orig_end++;
        }

        out[m][0] = orig_start;
        out[m][1] = (orig_end < (int)olen) ? orig_end : (int)olen;
    }

    free(orig_to_norm);
    free(norm_to_orig_start);
    free(norm_to_orig_end);
    return nmatch;
}
