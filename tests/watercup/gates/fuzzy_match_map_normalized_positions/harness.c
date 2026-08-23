/* gate: fuzzy_match_map_normalized_positions — traced copy of
 * src/tools/fuzzy_match.c::fuzzy_map_normalized_positions (+ span_t parse).
 * LIVE twin: tools/fuzzy_match.py::_map_normalized_positions.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: no normalized matches -> []
 *   LOOP build orig_to_norm:
 *     B2 chars equal / B3 ws-vs-space collapse / B4 extra-ws / B5 mismatch
 *   LOOP fill remaining -> nlen
 *   LOOP invert to norm_to_orig_{start,end}
 *   LOOP per match:
 *     B6 start in map / (else nearest)
 *     B7 end-1 in map / (else len-based)
 *     LOOP trailing-ws expansion (only when normalized match ends with space,
 *          guarded by B8)
 *
 * Input encoding (argv-safe): "<original>|<normalized>|<s:e,...>"
 * with an empty third field meaning zero matches.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

typedef struct { int start, end; } span_t;

static int count_char(const char *s, char c) {
    int n = 0; for (; *s; s++) if (*s == c) n++;
    return n;
}

static void wc_map_normalized_positions(const char *original,
                                        const char *normalized,
                                        const char *matches_spec) {
    WCUP_BEGIN("map_normalized_positions");
    int olen = (int)strlen(original);
    int nlen = (int)strlen(normalized);

    /* parse matches_spec "s:e,s:e" */
    span_t matches[64];
    int n_matches = 0;
    if (matches_spec[0]) {
        const char *p = matches_spec;
        while (*p) {
            int s = atoi(p);
            p = strchr(p, ':'); if (!p) break;
            int e = atoi(p + 1);
            matches[n_matches].start = s;
            matches[n_matches].end = e;
            n_matches++;
            p = strchr(p, ',');
            if (!p) break;
            p++;
        }
    }

    if (n_matches == 0) { WCUP_BRANCH(1); goto out; }   /* B1 */
    WCUP_BRANCH(0);

    {
        int *orig_to_norm = calloc((size_t)(olen + 1), sizeof(int));
        int orig_idx = 0, norm_idx = 0;
        while (orig_idx < olen && norm_idx < nlen) {
            WCUP_LOOP_TICK();
            if (original[orig_idx] == normalized[norm_idx]) {
                WCUP_BRANCH(1);                          /* B2 */
                orig_to_norm[orig_idx] = norm_idx;
                orig_idx++; norm_idx++;
            } else if ((original[orig_idx] == ' ' || original[orig_idx] == '\t') &&
                       normalized[norm_idx] == ' ') {
                WCUP_BRANCH(0); WCUP_BRANCH(1);          /* !B2, B3 */
                orig_to_norm[orig_idx] = norm_idx;
                orig_idx++;
                if (orig_idx < olen && original[orig_idx] != ' ' &&
                    original[orig_idx] != '\t')
                    norm_idx++;
            } else if (original[orig_idx] == ' ' || original[orig_idx] == '\t') {
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); /* B4 */
                orig_to_norm[orig_idx] = norm_idx;
                orig_idx++;
            } else {
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); /* B5 */
                orig_to_norm[orig_idx] = norm_idx;
                orig_idx++;
            }
        }
        while (orig_idx < olen) {
            WCUP_LOOP_TICK();                            /* fill remaining */
            orig_to_norm[orig_idx] = nlen;
            orig_idx++;
        }

        int *norm_to_orig_start = malloc((size_t)(nlen + 1) * sizeof(int));
        int *norm_to_orig_end = malloc((size_t)(nlen + 1) * sizeof(int));
        for (int i = 0; i <= nlen; i++) {
            norm_to_orig_start[i] = -1;
            norm_to_orig_end[i] = -1;
        }
        for (int i = 0; i < olen; i++) {
            WCUP_LOOP_TICK();                            /* invert loop */
            int np = orig_to_norm[i];
            if (norm_to_orig_start[np] == -1) norm_to_orig_start[np] = i;
            norm_to_orig_end[np] = i;
        }

        for (int m = 0; m < n_matches; m++) {
            WCUP_LOOP_TICK();                            /* per-match loop */
            int ns = matches[m].start, ne = matches[m].end;
            int orig_start, orig_end;
            if (ns <= nlen && norm_to_orig_start[ns] != -1) {
                WCUP_BRANCH(1);                          /* B6 */
                orig_start = norm_to_orig_start[ns];
            } else {
                WCUP_BRANCH(0);
                int best = olen;
                for (int i = 0; i < olen; i++) {
                    if (orig_to_norm[i] >= ns && orig_to_norm[i] < best)
                        best = orig_to_norm[i];
                }
                orig_start = best;
            }
            if ((ne - 1) <= nlen && ne - 1 >= 0 &&
                norm_to_orig_end[ne - 1] != -1) {
                WCUP_BRANCH(1);                          /* B7 */
                orig_end = norm_to_orig_end[ne - 1] + 1;
            } else {
                WCUP_BRANCH(0);
                orig_end = orig_start + (ne - ns);
            }
            if (ne < nlen && ne >= 1 && normalized[ne - 1] == ' ') {
                WCUP_BRANCH(1);                          /* B8 */
                while (orig_end < olen &&
                       (original[orig_end] == ' ' || original[orig_end] == '\t')) {
                    WCUP_LOOP_TICK();
                    orig_end++;
                }
            } else {
                WCUP_BRANCH(0);
            }
            if (orig_end > olen) orig_end = olen;
            (void)orig_start; (void)orig_end;
        }
        free(orig_to_norm); free(norm_to_orig_start); free(norm_to_orig_end);
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("map_normalized_positions");
    }
}

static void run_one(const char *spec) {
    /* split on '|' into 3 fields */
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", spec);
    char *f1 = strchr(buf, '|');
    char *f2 = NULL, *f3 = NULL;
    if (f1) { *f1 = 0; f2 = f1 + 1; f3 = strchr(f2, '|'); }
    if (f3) { *f3 = 0; f3++; } else { f3 = (char*)""; }
    if (!f2) f2 = (char*)"";
    wc_map_normalized_positions(buf, f2, f3);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s map_normalized_positions <orig|norm|matches>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    (void)count_char;
    return 0;
}
