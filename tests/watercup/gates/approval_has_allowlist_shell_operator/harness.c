#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
/* gate: traced copy of src/tools/approval_ports.c::apr_has_allowlist_shell_operator
 * (PoP: tools/approval.py:_has_allowlist_shell_operator). Quote-aware refinement
 * of LIVE Python is NOT ported in C; this gate covers the agreed unquoted-path
 * subset (see gate.json notes). */
static void run_one(const char *command) {
    int res = 0;
    WCUP_BEGIN("has_allowlist_shell_operator");
    if (!command) { WCUP_BRANCH(1); res = 0; }
    else {
        WCUP_BRANCH(0);
        res = 0;
        for (const char *p = command; *p; p++) {
            WCUP_LOOP_TICK();
            if (*p == '\n') { WCUP_BRANCH(1); res = 1; break; }
            WCUP_BRANCH(0);
            if (*p == '&' && p[1] == '&') { WCUP_BRANCH(1); res = 1; break; }
            WCUP_BRANCH(0);
            if (*p == '|' && p[1] == '|') { WCUP_BRANCH(1); res = 1; break; }
            WCUP_BRANCH(0);
            if (*p == ';' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '`') { WCUP_BRANCH(1); res = 1; break; }
            WCUP_BRANCH(0);
            if (*p == '$') {
                WCUP_BRANCH(1);
                if (p[1] == '(') { WCUP_BRANCH(1); res = 1; break; }
                WCUP_BRANCH(0);
            } else WCUP_BRANCH(0);
        }
    }
    char v[8]; snprintf(v, sizeof v, "%s", res ? "True" : "False");
    WCUP_VALSTR(v);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("has_allowlist_shell_operator");
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s has_allowlist_shell_operator <cmd>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(strcmp(argv[i], "\x00") == 0 ? NULL : argv[i]);
    return 0;
}
