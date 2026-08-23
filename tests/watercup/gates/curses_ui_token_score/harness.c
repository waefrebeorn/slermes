/* gate: curses_ui_token_score — traced C copy of
 * src/hermes_cli/curses_ui.c::curses_token_score (+ is_word_boundary).
 * LIVE twin: hermes_cli/curses_ui.py::_token_score.
 *
 * Rung ladder (identical on both sides):
 *   per token char: B1 not-found | B2 contiguous(+5) | B3 prev>=0 penalty |
 *                   B4 boundary(+3) | B5 idx==0(+5)
 *   then: B6 prefix bonus | B7 exact match | V(score)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static const char *WORD_BOUNDARY = "-_/. ";

static int wc_is_word_boundary(const char *target, int index) {
    if (index == 0) return 1;
    char prev = target[index - 1];
    for (const char *b = WORD_BOUNDARY; *b; b++) {
        if (prev == *b) return 1;
    }
    char cur = target[index];
    if (islower((unsigned char)prev) && isupper((unsigned char)cur)) return 1;
    return 0;
}

static double wc_token_score(const char *orig, const char *lower,
                             const char *token) {
    WCUP_BEGIN("token_score");
    double score = 0.0;
    if (!orig || !lower || !token || !*token) {
        /* degenerate: both sides treat as no-match */
        WCUP_BRANCH(1);
        score = -1.0;
        goto out;
    }
    WCUP_BRANCH(0);
    {
        int prev = -1;
        int search_from = 0;
        int positions[256];
        int npos = 0;

        for (int j = 0; token[j]; j++) {
            char ch = token[j];
            char *found = strchr(lower + search_from, ch);
            if (!found) { WCUP_BRANCH(1); goto nomatch; }
            WCUP_BRANCH(0);
            int idx = (int)(found - lower);
            if (npos < 256) positions[npos++] = idx;
            score += 1;
            WCUP_BRANCH(prev >= 0 && idx == prev + 1);      /* B2 */
            if (prev >= 0 && idx == prev + 1) {
                score += 5;
            } else {
                WCUP_BRANCH(prev >= 0);                      /* B3 */
                if (prev >= 0) score -= (idx - prev - 1 < 3) ? (idx - prev - 1) : 3;
            }
            WCUP_BRANCH(wc_is_word_boundary(orig, idx));     /* B4 */
            if (wc_is_word_boundary(orig, idx)) score += 3;
            WCUP_BRANCH(idx == 0);                           /* B5 */
            if (idx == 0) score += 5;
            prev = idx;
            search_from = idx + 1;
        }
        WCUP_BRANCH(npos > 0 && positions[0] == 0 &&
                    positions[npos-1] == npos - 1);          /* B6 */
        if (npos > 0 && positions[0] == 0 && positions[npos-1] == npos - 1)
            score += 8;
        WCUP_BRANCH(strcmp(lower, token) == 0);              /* B7 */
        if (strcmp(lower, token) == 0) score += 20;
        score -= (double)strlen(lower) * 0.01;
        goto out;
    }
nomatch:
    score = -1.0;
out:
    {
        char vbuf[64];
        snprintf(vbuf, sizeof vbuf, "%.17g", score);
        WCUP_VALSTR(vbuf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("token_score");
        return score;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s token_score <label|token>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *bar = strchr(argv[i], '|');
        if (!bar) { fprintf(stderr, "bad input (need label|token): %s\n", argv[i]); return 2; }
        *bar = '\0';
        const char *orig = argv[i];
        char lower[1024];
        size_t k = 0;
        for (; orig[k] && k < sizeof(lower) - 1; k++)
            lower[k] = (char)tolower((unsigned char)orig[k]);
        lower[k] = '\0';
        wc_token_score(orig, lower, bar + 1);
    }
    return 0;
}
