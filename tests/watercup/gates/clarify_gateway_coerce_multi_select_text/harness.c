#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"
/* gate: traced copy of src/tools/clarify_gateway.c::
 * cli_tools_clarify_gateway__coerce_multi_select_text
 * (PoP: tools/clarify_gateway.py:_coerce_multi_select_text).
 * Input encoding: choicesCSV \x1f replyText. Value: chosen labels joined '|'
 * or "None" (NULL reply). */
static int ci_eq(const char *a, const char *b) {
    while (*a && *b) { if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0; a++; b++; }
    return *a == '\0' && *b == '\0';
}
static void coerce_one(const char *choices_csv, const char *text) {
    char cbuf[1024]; snprintf(cbuf, sizeof cbuf, "%s", choices_csv);
    char *choices[32]; int nc = 0;
    for (char *t = strtok(cbuf, ","); t && nc < 32; t = strtok(NULL, ",")) choices[nc++] = t;
    char out[2048]; out[0] = '\0';
    int ok = 1;
    WCUP_BEGIN("coerce_multi_select_text");
    if (!text || !*text || nc <= 0) { WCUP_BRANCH(1); ok = 0; }
    else {
        WCUP_BRANCH(0);
        char work[2048]; size_t n = 0;
        for (const char *p = text; *p && n + 1 < sizeof(work); p++)
            if (!isspace((unsigned char)*p) || n > 0) work[n++] = *p;
        while (n > 0 && isspace((unsigned char)work[n-1])) n--;
        work[n] = '\0';
        char *tokens[64]; int ntok = 0;
        if (strchr(work, ',')) {
            WCUP_BRANCH(1);
            char *save = NULL;
            for (char *tok = strtok_r(work, ",", &save); tok && ntok < 64; tok = strtok_r(NULL, ",", &save)) {
                char *s = tok; while (*s && isspace((unsigned char)*s)) s++;
                char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) e--;
                if (e > s) { *e = '\0'; tokens[ntok++] = s; }
            }
        } else {
            WCUP_BRANCH(0);
            char *save2 = NULL;
            char *toks[64]; int nt2 = 0; int allnum = 1;
            for (char *tok = strtok_r(work, " \t", &save2); tok && nt2 < 64; tok = strtok_r(NULL, " \t", &save2)) {
                toks[nt2++] = tok;
                if (!allnum || !*tok) allnum = 0;
                for (char *q = tok; *q; q++) if (!isdigit((unsigned char)*q)) allnum = 0;
            }
            if (nt2 > 1 && allnum) {
                WCUP_BRANCH(1);
                for (int i = 0; i < nt2; i++) tokens[ntok++] = toks[i];
            } else {
                WCUP_BRANCH(0);
                if (nt2 > 0) tokens[ntok++] = toks[0];
            }
        }
        for (int tix = 0; tix < ntok && ok; tix++) {
            const char *tok = tokens[tix];
            int isnum = tok[0] && tok[strspn(tok, "0123456789")] == '\0';
            if (isnum) {
                WCUP_BRANCH(1);
                long idx = atol(tok);
                if (idx >= 1 && idx <= nc) {
                    WCUP_BRANCH(1);
                    if (out[0]) strcat(out, "|");
                    strcat(out, choices[idx - 1]);
                } else { WCUP_BRANCH(0); ok = 0; }
            } else {
                WCUP_BRANCH(0);
                int found = 0;
                for (int i = 0; i < nc; i++) {
                    WCUP_LOOP_TICK();
                    if (strlen(tok) == strlen(choices[i])) {
                        WCUP_BRANCH(1);
                        if (ci_eq(tok, choices[i])) {
                            WCUP_BRANCH(1);
                            if (out[0]) strcat(out, "|");
                            strcat(out, choices[i]);
                            found = 1; break;
                        } else WCUP_BRANCH(0);
                    } else WCUP_BRANCH(0);
                }
                if (!found) ok = 0;
            }
        }
    }
    if (!ok) strcpy(out, "None");
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("coerce_multi_select_text");
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coerce_multi_select_text <choices\\x1ftext>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *s = strdup(argv[i]);
        char *sep = strchr(s, '\x1f');
        if (sep) { *sep = '\0'; coerce_one(s, sep + 1); }
        else coerce_one(s, "");
        free(s);
    }
    return 0;
}
