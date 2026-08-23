#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
/* gate: traced copy of src/agent/context_compressor_pure.c::cc_is_blank_user_turn
 * (PoP: agent/context_compressor.py:_is_blank_user_turn).
 * Input encoding: meta\x1frole\x1fcontent where content is:
 *   N            -> null
 *   S:<text>     -> string content
 *   A:<p>,<p>    -> array of parts; part = S:<text> | T:<text> | I (image) | U:<type>
 * Empty-array encoded as "A:".
 * NOTE: C tests emptiness with strlen==0, LIVE with .strip() — whitespace-only
 * text inputs are excluded (documented divergence). Summary-content detection
 * is out of the C port's traced surface; only the metadata flag is exercised. */
static void run_one(const char *meta_s, const char *role, const char *spec) {
    int res = 0;
    WCUP_BEGIN("is_blank_user_turn");
    if (strcmp(role, "user") != 0) { WCUP_BRANCH(1); }
    else {
        WCUP_BRANCH(0);
        if (meta_s[0] == '1') { WCUP_BRANCH(1); }
        else {
            WCUP_BRANCH(0);
            const char *c = spec;
            int is_null = strcmp(c, "N") == 0;
            int is_str = c[0] == 'S' && c[1] == ':';
            int is_arr = c[0] == 'A' && (c[1] == ':' || c[1] == '\0');
            if (is_null) {
                WCUP_BRANCH(1);
                res = 1; /* content==NULL -> blank */
            } else {
                WCUP_BRANCH(0);
                if (is_str) {
                    WCUP_BRANCH(1);
                    const char *t = c + 2;
                    if (strlen(t) == 0) res = 1;
                } else {
                    WCUP_BRANCH(0);
                    if (!is_arr) { WCUP_BRANCH(1); }
                    else {
                        WCUP_BRANCH(0);
                        const char *parts = (c[1] == ':') ? c + 2 : c + 1;
                        if (*parts == '\0') { WCUP_BRANCH(1); res = 1; }
                        else {
                            WCUP_BRANCH(0);
                            res = 1; /* all-blank assumption */
                            char buf[2048]; snprintf(buf, sizeof buf, "%s", parts);
                            for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
                                if (tok[0] == 'S' && tok[1] == ':') {
                                    WCUP_BRANCH(1);
                                    if (strlen(tok + 2) > 0) { res = 0; break; }
                                } else if ((tok[0] == 'T' && tok[1] == ':')) {
                                    WCUP_BRANCH(0); /* first branch false */
                                    WCUP_BRANCH(strcmp(tok, "T:") == 0 ? 1 : 0);
                                    /* text block: type in {text,input_text} */
                                    if (strlen(tok + 2) > 0) { res = 0; break; }
                                } else {
                                    WCUP_BRANCH(0);
                                    WCUP_BRANCH(0);
                                    res = 0; /* image/audio/unknown block */
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    char v[4]; snprintf(v, sizeof v, "%d", res);
    WCUP_VALSTR(v);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_blank_user_turn");
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_blank_user_turn <meta\\x1frole\\x1fspec>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *s = strdup(argv[i]);
        char *f1 = strchr(s, '\x1f'); if (f1) *f1 = 0;
        char *f2 = f1 ? strchr(f1+1, '\x1f') : NULL; if (f2) *f2 = 0;
        run_one(s, f1 ? f1+1 : "", f2 ? f2+1 : "N");
        free(s);
    }
    return 0;
}
