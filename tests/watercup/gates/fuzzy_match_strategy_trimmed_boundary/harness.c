/* gate: fuzzy_match_strategy_trimmed_boundary — traced C copy of
 * src/hermes_cli/fuzzy_match_helpers.c::fuzzy_match_strategy_trimmed_boundary.
 * Twin: LIVE tools/fuzzy_match.py::_strategy_trimmed_boundary.
 * SCOPE: single-line patterns only. The C port trims EVERY line of the
 * pattern/block while LIVE trims only first and last — they diverge on
 * multi-line patterns (documented in gate.json), so the walk pins np==1,
 * where trimming-all equals trimming-first.
 * Input encoding: "<pattern>|||<content>".
 * Trace contract: b0 empty-pattern-list; per window LOOP tick;
 * b1 multi-line last-trim (always False at np==1); b2 block matches.
 * Value: comma-joined "start:end" match positions ("" for none).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static char *trim(char *s) {
    size_t L = strlen(s);
    size_t a = 0;
    while (a < L && (s[a] == ' ' || s[a] == '\t')) a++;
    size_t b = L;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t')) b--;
    s[b] = '\0';
    return s + a;
}

/* split content into lines (Python str.split('\n') semantics: trailing
 * newline yields a final empty line) */
static int split_nl(const char *s, char lines[][256], int max) {
    int n = 0;
    const char *p = s;
    for (;;) {
        const char *eol = strchr(p, '\n');
        size_t ll = eol ? (size_t)(eol - p) : strlen(p);
        if (n < max) { memcpy(lines[n], p, ll < 255 ? ll : 255); lines[n][ll < 255 ? ll : 255] = '\0'; }
        n++;
        if (!eol) break;
        p = eol + 1;
    }
    return n;
}

static void run_one(const char *pattern, const char *content) {
    WCUP_BEGIN("strategy_trimmed_boundary");
    char out[512] = "";
    char plines[8][256], clines[64][256];
    /* pattern is single-line by scope */
    strcpy(plines[0], pattern);
    int np = 1;
    WCUP_BRANCH(np == 0);                          /* b0: empty pattern list */
    if (np != 0) {
        char modified[256];
        strcpy(modified, plines[0]);
        char *m = trim(modified);
        int nc = split_nl(content, clines, 64);
        int nm = 0;
        for (int i = 0; i + np <= nc; i++) {
            WCUP_LOOP_TICK();
            char check[256];
            strcpy(check, clines[i]);
            char *t = trim(check);
            WCUP_BRANCH(0);                        /* b1: multi-line last-trim */
            int is_match = (strcmp(t, m) == 0);
            WCUP_BRANCH(is_match);                 /* b2 */
            if (is_match) {
                long start = 0, end;
                for (int k = 0; k < i; k++) start += (long)strlen(clines[k]) + 1;
                end = start + (long)strlen(clines[i]) + 1 - 1;
                long clen = (long)strlen(content);
                if (end > clen) end = clen;
                nm += snprintf(out + nm, sizeof out - nm, nm ? ",%ld:%ld" : "%ld:%ld", start, end);
            }
        }
    }
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("strategy_trimmed_boundary");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strategy_trimmed_boundary <pat|||content>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[4096];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *sep = strstr(buf, "|||");
        if (!sep) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *sep = '\0';
        run_one(buf, sep + 3);
    }
    return 0;
}
