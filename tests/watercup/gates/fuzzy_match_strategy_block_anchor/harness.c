/* gate: fuzzy_match_strategy_block_anchor
 * Calls the REAL C port src/hermes_cli/fuzzy_match_helpers.c::
 * fuzzy_match_strategy_block_anchor against LIVE Python
 * tools/fuzzy_match.py::_strategy_block_anchor.
 *
 * Input argv format: "<pattern>\\t<content>" where \\n inside either field
 * encodes a newline (literal backslash-n in the shell arg, decoded here).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

void fuzzy_match_strategy_block_anchor(const char *content, const char *pattern,
                                       long **matches, size_t *nmatch);

static void decode(char *s) {
    /* turn literal "\n" into real newlines, in place */
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 'n') { *o++ = '\n'; p++; }
        else if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *input) {
    WCUP_BEGIN("_strategy_block_anchor");
    char tmp[16384];
    snprintf(tmp, sizeof tmp, "%s", input ? input : "");
    decode(tmp);
    char *tab = strchr(tmp, '\t');
    if (!tab) { fprintf(stderr, "bad input\n"); return; }
    *tab = '\0';
    const char *pattern = tmp;
    const char *content = tab + 1;

    long *matches = NULL;
    size_t nmatch = 0;
    fuzzy_match_strategy_block_anchor(content, pattern, &matches, &nmatch);

    /* Trace LIVE's ladder over the same fields. */
    char pc[4096], cc[16384];
    snprintf(pc, sizeof pc, "%s", pattern);
    snprintf(cc, sizeof cc, "%s", content);
    int plines = 1;
    for (char *p = pc; *p; p++) if (*p == '\n') plines++;
    WCUP_BRANCH(plines < 2);
    if (plines >= 2) {
        /* first/last pattern lines (stripped) */
        char first[512] = "", last[512] = "";
        {
            char *a = pc;
            char *b = strchr(pc, '\n');
            size_t n = b ? (size_t)(b - a) : strlen(a);
            size_t k = 0;
            size_t st = 0, en = n;
            while (st < en && (a[st]==' '||a[st]=='\t')) st++;
            while (en > st && (a[en-1]==' '||a[en-1]=='\t')) en--;
            for (size_t z = st; z < en && k < 511; z++) first[k++] = a[z];
            first[k] = 0;
            char *l0 = b ? b + 1 : a;
            size_t L = strlen(l0), st2 = 0, en2 = L;
            while (st2 < en2 && (l0[st2]==' '||l0[st2]=='\t')) st2++;
            while (en2 > st2 && (l0[en2-1]==' '||l0[en2-1]=='\t')) en2--;
            k = 0;
            for (size_t z = st2; z < en2 && k < 511; z++) last[k++] = l0[z];
            last[k] = 0;
        }
        int clines = 1;
        for (char *p = cc; *p; p++) if (*p == '\n') clines++;
        long ncand = 0;
        if (clines >= plines) {
            for (int i = 0; i + plines <= clines; i++) {
                /* extract line i and line i+plines-1 from cc, strip, compare */
                char ln[1024]; int li = 0, cur = 0;
                char fline[1024] = "", lline[1024] = "";
                int target_a = i, target_b = i + plines - 1;
                for (char *p = cc; ; p++) {
                    if (*p == '\n' || !*p) {
                        ln[li] = 0;
                        if (cur == target_a || cur == target_b) {
                            size_t st = 0, en = li;
                            while (st < en && (ln[st]==' '||ln[st]=='\t')) st++;
                            while (en > st && (ln[en-1]==' '||ln[en-1]=='\t')) en--;
                            char *dst = (cur == target_a) ? fline : lline;
                            size_t k = 0;
                            for (size_t z = st; z < en && k < 1023; z++) dst[k++] = ln[z];
                            dst[k] = 0;
                        }
                        cur++; li = 0;
                        if (!*p) break;
                        continue;
                    }
                    if (li < 1023) ln[li++] = *p;
                }
                if (strcmp(fline, first) == 0) {
                    WCUP_BRANCH(1);
                    if (strcmp(lline, last) == 0) { WCUP_BRANCH(1); ncand++; }
                    else WCUP_BRANCH(0);
                } else {
                    WCUP_BRANCH(0);
                }
            }
        }
        WCUP_BRANCH(ncand == 1);
        for (long cidx = 0; cidx < ncand; cidx++) {
            WCUP_BRANCH(plines <= 2);          /* similarity 1.0 vs seq_ratio */
            WCUP_BRANCH(matches != NULL);      /* threshold pass (V below carries truth) */
        }
    }

    char res[1024];
    size_t o = 0;
    res[0] = 0;
    for (size_t m = 0; m < nmatch; m++) {
        o += (size_t)snprintf(res + o, sizeof res - o, "%s%ld-%ld", m ? ";" : "", matches[2*m], matches[2*m+1]);
    }
    if (!o) snprintf(res, sizeof res, "[]");
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_strategy_block_anchor");
    free(matches);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _strategy_block_anchor <pattern>\\t<content>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
