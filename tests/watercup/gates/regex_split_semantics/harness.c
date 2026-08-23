/* gate: regex_split_semantics — traced driver over the REAL
 * lib/libregex/hermes_regex.c::regex_split (LIVE twin: python re.split).
 *
 * Probe encoding: "<maxsplit>|<pattern>|<subject>"
 *
 * Rung order (must match twin.py EXACTLY), per split step:
 *   S1: maxsplit limit reached -> remainder is final field
 *   S2: no further match       -> remainder is final field
 *   S3: zero-width match       -> consume one char
 *   V: all fields joined with \x1f (from the REAL regex_split output)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include "watercup.h"
#include "libregex/hermes_regex.h"

#define MAXF 64

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fn <probe>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[4096];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *p = probe;
        char *f1 = strsep(&p, "|");
        char *pat = strsep(&p, "|");
        char *subj = p ? p : (char*)"";
        int maxsplit = f1 ? atoi(f1) : -1;

        WCUP_BEGIN("regex_split_semantics");

        /* traced mirror of regex_split's decision loop
         * (pos = search cursor, last = pending segment start) */
        hregex_t *re = regex_compile(pat, 0);
        size_t slen = strlen(subj);
        size_t pos = 0, last = 0;
        int splits = 0;
        for (;;) {
            int last_field = (maxsplit >= 0 && splits >= maxsplit);
            regex_match_t *m = NULL;
            if (last_field) {                            /* S1 */
                WCUP_BRANCH(1);
                break;
            }
            WCUP_BRANCH(0);
            m = re ? regex_search(re, subj + (int)pos) : NULL;
            if (!m || !m->matched || m->start[0] < 0) {  /* S2 */
                WCUP_BRANCH(1);
                regex_match_free(m);
                break;
            }
            WCUP_BRANCH(0);
            size_t mstart = pos + (size_t)m->start[0];
            size_t mend   = pos + (size_t)m->end[0];
            if (mend == mstart) {                        /* S3 zero-width */
                WCUP_BRANCH(1);
                regex_match_free(m);
                if (mend >= slen) { last = slen + 1; break; }  /* trailing "" */
                last = mend;
                pos = mend + 1;
            } else {
                WCUP_BRANCH(0);
                regex_match_free(m);
                last = pos = mend;
            }
            splits++;
        }

        /* real shipped output pinned as the value event */
        {
            char *fields[MAXF];
            int n = re ? regex_split(re, subj, maxsplit, fields, MAXF) : 0;
            char joined[8192];
            size_t o = 0;
            joined[0] = '\0';
            for (int k = 0; k < n; k++) {
                if (k) joined[o++] = '\x1f';
                size_t l = strlen(fields[k]);
                if (o + l + 1 < sizeof joined) { memcpy(joined + o, fields[k], l); o += l; }
                joined[o] = '\0';
            }
            WCUP_VALSTR(joined);
            if (re) regex_split_free(fields, n);
        }
        if (re) regex_free(re);

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("regex_split_semantics");
    }
    return 0;
}
