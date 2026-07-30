/*
 * port_tools_fuzzy_match.c — C port of tools/fuzzy_match.py
 *
 * _map_normalized_positions(original, normalized, normalized_matches):
 * map (start,end) match spans from a whitespace-normalized string back to
 * positions in the original string. Faithful to LIVE Python including the
 * trailing-whitespace expansion rule (issue #52491).
 *
 * The Python algorithm builds orig_to_norm[i] = normalized position for each
 * original char, then inverts it to map normalized positions back to original
 * ranges, then expands the end when the normalized match ended in whitespace
 * (so a match like "foo " includes the trailing space that was collapsed).
 *
 * Verified byte-equal to LIVE Python via tests/sta_oracle_fuzzy_match.py.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int start; int end; } span_t;

/*
 * Returns a malloc'd array of spans (count written to *out_n) mapping each
 * normalized match back to (start,end) positions in `original`.
 * A normalized match (ns, ne) becomes (orig_start, orig_end) where orig_end
 * may be expanded to consume trailing whitespace collapsed during normalize.
 */
/* PoP: fuzzy_map_normalized_positions @ tools/fuzzy_match.py:_map_normalized_positions */
span_t *fuzzy_map_normalized_positions(const char *original,
                                       const char *normalized,
                                       const span_t *matches,
                                       int n_matches,
                                       int *out_n) {
    int olen = (int)strlen(original);
    int nlen = (int)strlen(normalized);

    /* orig_to_norm[i] = normalized position of original char i */
    int *orig_to_norm = calloc((size_t)(olen + 1), sizeof(int));
    int orig_idx = 0, norm_idx = 0;
    while (orig_idx < olen && norm_idx < nlen) {
        if (original[orig_idx] == normalized[norm_idx]) {
            orig_to_norm[orig_idx] = norm_idx;
            orig_idx++; norm_idx++;
        } else if ((original[orig_idx] == ' ' || original[orig_idx] == '\t') &&
                   normalized[norm_idx] == ' ') {
            orig_to_norm[orig_idx] = norm_idx;
            orig_idx++;
            if (orig_idx < olen && original[orig_idx] != ' ' && original[orig_idx] != '\t')
                norm_idx++;
        } else if (original[orig_idx] == ' ' || original[orig_idx] == '\t') {
            orig_to_norm[orig_idx] = norm_idx;
            orig_idx++;
        } else {
            orig_to_norm[orig_idx] = norm_idx;
            orig_idx++;
        }
    }
    while (orig_idx < olen) {
        orig_to_norm[orig_idx] = nlen;
        orig_idx++;
    }

    /* invert: norm -> (start,end) original range */
    int *norm_to_orig_start = malloc((size_t)(nlen + 1) * sizeof(int));
    int *norm_to_orig_end = malloc((size_t)(nlen + 1) * sizeof(int));
    for (int i = 0; i <= nlen; i++) { norm_to_orig_start[i] = -1; norm_to_orig_end[i] = -1; }
    for (int i = 0; i < olen; i++) {
        int np = orig_to_norm[i];
        if (norm_to_orig_start[np] == -1) norm_to_orig_start[np] = i;
        norm_to_orig_end[np] = i;
    }

    span_t *out = malloc((size_t)(n_matches > 0 ? n_matches : 1) * sizeof(span_t));
    int on = 0;
    for (int m = 0; m < n_matches; m++) {
        int ns = matches[m].start, ne = matches[m].end;
        int orig_start, orig_end;
        if (ns <= nlen && norm_to_orig_start[ns] != -1) {
            orig_start = norm_to_orig_start[ns];
        } else {
            /* nearest normalized position >= ns */
            int best = olen;
            for (int i = 0; i < olen; i++) {
                if (orig_to_norm[i] >= ns && orig_to_norm[i] < best) best = orig_to_norm[i];
            }
            orig_start = best;
        }
        if ((ne - 1) <= nlen && ne - 1 >= 0 && norm_to_orig_end[ne - 1] != -1) {
            orig_end = norm_to_orig_end[ne - 1] + 1;
        } else {
            orig_end = orig_start + (ne - ns);
        }
        /* trailing-whitespace expansion (issue #52491) */
        if (ne < nlen && ne >= 1 && normalized[ne - 1] == ' ') {
            while (orig_end < olen && (original[orig_end] == ' ' || original[orig_end] == '\t'))
                orig_end++;
        }
        if (orig_end > olen) orig_end = olen;
        out[on].start = orig_start;
        out[on].end = orig_end;
        on++;
    }

    free(orig_to_norm);
    free(norm_to_orig_start);
    free(norm_to_orig_end);
    *out_n = on;
    return out;
}
