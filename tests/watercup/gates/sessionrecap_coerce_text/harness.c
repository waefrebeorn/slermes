/* gate: sessionrecap_coerce_text — traced copy of
 * src/hermes_cli/session_recap.c::session_coerce_text
 * (PoP: hermes_cli/session_recap.py:_coerce_text).
 * Input encoding (\x1f-separated tokens): first token is kind:
 *   N          -> JSON null / Python None
 *   S:<text>   -> string value
 *   A <p>...   -> array of blocks; part = S:<text> | T:<text> | TN | E
 *                 (T: dict {"text": ...}, TN dict with text="", E dict w/o text)
 * NOTE: LIVE falls back to str(value) for scalars; C prints a type word —
 * scalar-number inputs are excluded (documented divergence).*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void run_one(int argc, char **argv) {
    char out[4096]; size_t off = 0; out[0] = '\0';
    int have = 0;
    WCUP_BEGIN("coerce_text");
    const char *kind = argv[0];
    int is_null = strcmp(kind, "N") == 0;
    int is_str = kind[0] == 'S' && kind[1] == ':';
    if (!kind[0]) { WCUP_BRANCH(1); }
    else {
        WCUP_BRANCH(0);
        if (is_null) {
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
            if (is_str) {
                WCUP_BRANCH(1);
                const char *t = kind + 2;
                snprintf(out, sizeof out, "%s", t);
            } else {
                WCUP_BRANCH(0);
                /* array walk */
                for (int i = 2; i < argc; i++) {
                    WCUP_LOOP_TICK();
                    const char *b = argv[i];
                    const char *text = NULL;
                    int is_s = b[0]=='S' && b[1]==':';
                    if (!b[0]) { WCUP_BRANCH(1); continue; }
                    WCUP_BRANCH(0);
                    if (is_s) { WCUP_BRANCH(1); text = b + 2; }
                    else {
                        WCUP_BRANCH(0);
                        if (b[0]=='T') { WCUP_BRANCH(1); text = b[2] ? b+2 : ""; }
                        else WCUP_BRANCH(0);
                    }
                    /* LIVE: str blocks always collected; dict blocks need truthy text */
                    if (text && (is_s || text[0])) {
                        WCUP_BRANCH(1);
                        if (have) out[off++] = '\n';
                        strcpy(out + off, text); off += strlen(text);
                        have = 1;
                    } else WCUP_BRANCH(0);
                }
            }
        }
    }
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("coerce_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coerce_text <inputs...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        /* split input on the unit separator into tokens */
        char *copy = strdup(argv[i]);
        char *toks[256]; int nt = 0;
        char *save = NULL;
        for (char *t = copy; t; ) {
            char *nx = strchr(t, 0x1f);
            if (nx) *nx = '\0';
            toks[nt++] = t;
            t = nx ? nx + 1 : NULL;
        }
        run_one(nt, toks);
        free(copy);
    }
    return 0;
}
