/* gate: contentcompressor_content_text_for_contains — traced copy of the FIXED
 * src/agent/context_compressor_pure.c::cc_content_text_for_contains
 * (PoP: agent/context_compressor.py:_content_text_for_contains).
 * Input encoding (\x1f tokens): N | S:<text> | A <p>... with part =
 *   S:<text> (string item) | TT:<text> ({"type":"text","text":...})
 *   TD:<text> ({"text":...} no type)  | TI:<text> ({"type":"image_url",...})
 *   TN ({"type":"text","text":""})    | E ({}) | X (string item in object? no — number)
 * PORT FIX (this batch): C now joins non-empty parts with '\n' and accepts any
 * dict with a str "text" — previously it concatenated raw and filtered on
 * type=="text", diverging from LIVE.
 * DOCUMENTED DIVERGENCE: LIVE does str(value) for other scalars; C returns ""
 * — scalar-number inputs excluded.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void run_one(int argc, char **argv) {
    char out[4096]; size_t len = 0; out[0] = '\0';
    WCUP_BEGIN("content_text_for_contains");
    const char *kind = argv[0];
    int is_null = strcmp(kind, "N") == 0;
    int is_str = kind[0] == 'S' && kind[1] == ':';
    if (!kind[0]) { WCUP_BRANCH(1); }
    else {
        WCUP_BRANCH(0);
        if (is_null) { WCUP_BRANCH(1); }
        else {
            WCUP_BRANCH(0);
            if (is_str) {
                WCUP_BRANCH(1);
                snprintf(out, sizeof out, "%s", kind + 2);
                len = strlen(out);
            } else {
                WCUP_BRANCH(0);
                for (int i = 2; i < argc; i++) {
                    WCUP_LOOP_TICK();
                    const char *b = argv[i];
                    if (!b[0]) { WCUP_BRANCH(1); continue; }
                    WCUP_BRANCH(0);
                    const char *seg = NULL;
                    int is_s = b[0]=='S' && b[1]==':';
                    if (is_s) { WCUP_BRANCH(1); seg = b + 2; }
                    else {
                        WCUP_BRANCH(0);
                        /* dict part: has a "text" payload unless E/X */
                        if (b[0]=='T' || b[0]=='D') { WCUP_BRANCH(1); seg = strlen(b) > 3 ? b + 3 : ""; }
                        else { WCUP_BRANCH(0); }
                    }
                    /* LIVE filters falsy parts at the join */
                    if (!seg || !*seg) { WCUP_BRANCH(0); continue; }
                    WCUP_BRANCH(1);
                    if (len > 0) { out[len++] = '\n'; out[len] = '\0'; }
                    memcpy(out + len, seg, strlen(seg)); len += strlen(seg); out[len]='\0';
                }
            }
        }
    }
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("content_text_for_contains");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s content_text_for_contains <inputs...>\n", argv[0]); return 2; }
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
