/* gate: fuzzy_strategy_ladder — traced driver over the REAL
 * lib/libfuzzymatch/fuzzy_match.c::fuzzy_find_and_replace
 * (LIVE twin: tools/fuzzy_match.py::fuzzy_find_and_replace).
 *
 * Probe encoding: "<content>\x1f<old>\x1f<new>\x1f<replace_all 0|1>"
 * (\x1f byte separates the four probe fields.)
 *
 * Rung order (must match twin.py EXACTLY):
 *   F1: identical strings short-circuit (already-applied / identical)
 *   F2: a strategy matched
 *   V: strategy index ("0" = no strategy)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "fuzzy_match.h"

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fn <probe>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[16384];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *p = probe;
        char *content = strsep(&p, "\x1f");
        char *old_s   = strsep(&p, "\x1f");
        char *new_s   = strsep(&p, "\x1f");
        char *ra_s    = p ? p : (char*)"0";
        int replace_all = atoi(ra_s);

        WCUP_BEGIN("fuzzy_strategy_ladder");

        /* F1: LIVE identical-strings guard fires before the chain */
        if (old_s && new_s && strcmp(old_s, new_s) == 0) {    /* F1 */
            WCUP_BRANCH(1);
            WCUP_VALSTR("identical");
        } else {
            WCUP_BRANCH(0);
            fuzzy_result_t r = fuzzy_find_and_replace(content, old_s,
                                                      new_s ? new_s : "",
                                                      replace_all);
            if (r.strategy > 0) {                             /* F2 */
                WCUP_BRANCH(1);
                char sb[16];
                snprintf(sb, sizeof sb, "%d", r.strategy);
                WCUP_VALSTR(sb);
            } else {
                WCUP_BRANCH(0);
                WCUP_VALSTR("0");
            }
            fuzzy_result_free(&r);
        }

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fuzzy_strategy_ladder");
    }
    return 0;
}
