/* gate: commands_score_path — traced copy of
 * src/hermes_cli/commands_wrappers.c::cmd_score_path
 * (PoP: hermes_cli/commands.py::_score_path).
 *
 * Input encoding: "<path>|<query>"
 *
 * Rung ladder (must match twin.py EXACTLY):
 *   B1 empty path / B2 empty query / B3 exact filename /
 *   B4 filename starts-with / B5 query in filename / B6 subsequence complete
 *   V(score)
 *
 * Documented divergences AVOIDED by input choice (see gate.json notes):
 *  - LIVE has a path-contains rung scoring 40 (C does not) -> no inputs hit it.
 *  - LIVE has a word-boundary bonus rung scoring 35 (C returns 25 directly)
 *    -> all subsequence-match inputs keep boundary_hits < len(q)*0.5.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static int trace_score_path(const char *path, const char *query) {
    WCUP_BEGIN("score_path");
    int score;
    WCUP_BRANCH(!path || !*path);                    /* B1 */
    if (!query || !*query) {                         /* B2 */
        WCUP_BRANCH(1);
        score = 1;
    } else {
        WCUP_BRANCH(0);
        size_t ql = strlen(query);
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        char lower[1024]; size_t w = 0;
        for (const char *p = base; *p && w < sizeof(lower) - 1; p++)
            lower[w++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
        lower[w] = '\0';
        char lq[128]; w = 0;
        for (const char *p = query; *p && w < sizeof(lq) - 1; p++)
            lq[w++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
        lq[w] = '\0';
        WCUP_BRANCH(strcmp(lower, lq) == 0);         /* B3 */
        if (strcmp(lower, lq) == 0) {
            score = 100;
        } else {
            WCUP_BRANCH(strncmp(lower, lq, ql) == 0);/* B4 */
            if (strncmp(lower, lq, ql) == 0) {
                score = 80;
            } else {
                WCUP_BRANCH(strstr(lower, lq) != NULL); /* B5 */
                if (strstr(lower, lq)) {
                    score = 60;
                } else {
                    size_t qi = 0;                   /* subsequence scan */
                    for (const char *c = lower; *c && qi < ql; c++)
                        if (*c == lq[qi]) qi++;
                    WCUP_BRANCH(qi == ql);           /* B6 */
                    score = (qi == ql) ? 25 : 0;
                }
            }
        }
    }
    char vs[32]; snprintf(vs, sizeof vs, "%d", score);
    WCUP_VALSTR(vs);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("score_path");
    return score;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_score_path <path|query>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[1024]; snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strrchr(probe, '|');
        if (!bar) { fprintf(stderr, "bad probe (need path|query): %s\n", probe); return 2; }
        *bar = '\0';
        trace_score_path(probe, bar + 1);
    }
    return 0;
}
