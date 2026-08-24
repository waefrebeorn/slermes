/*
 * relaunch.c — canonical C11 port of hermes_cli/relaunch.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_relaunch_u_build_inherited_flag_table @ hermes_cli/relaunch.py:_build_inherited_flag_table */
int hermes_cli_relaunch_u_build_inherited_flag_table(const char *arg) {
    /* Python: introspect parser actions. Arg = "table" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_relaunch_u_extract_inherited_flags @ hermes_cli/relaunch.py:_extract_inherited_flags */
int hermes_cli_relaunch_u_extract_inherited_flags(const char *arg) {
    /* Python: carry-over flags (+ value). Arg = "argv" (space-sep).
     * Table mirrors LIVE _INHERITED_FLAGS_TABLE (relaunch.py) exactly:
     * each entry is flag + takes_value. */
    static const struct { const char *flag; int takes_value; } flags_table[] = {
        {"-m", 1}, {"--model", 1}, {"--provider", 1}, {"--reasoning", 1},
        {"--accept-hooks", 0}, {"--skills", 1}, {"-s", 1}, {"--yolo", 0},
        {"--pass-session-id", 0}, {"--ignore-user-config", 0},
        {"--ignore-rules", 0}, {"--safe-mode", 0}, {"--tui", 0},
        {"--cli", 0}, {"--dev", 0}, {"--profile", 1}, {"-p", 1},
        {NULL, 0}
    };
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    char *toks[512]; int nt = 0;
    while (*p && nt < 512) {
        const char *sp = strchr(p, ' ');
        size_t len = sp ? (size_t)(sp - p) : strlen(p);
        if (len) {
            char *tok = strndup(p, len);
            toks[nt++] = tok;
        }
        p = sp ? sp + 1 : p + len;
    }
    int first = 1;
    for (int i = 0; i < nt; i++) {
        const char *tok = toks[i];
        const char *eq = strchr(tok, '=');
        if (eq) {
            size_t keylen = (size_t)(eq - tok);
            for (int k = 0; flags_table[k].flag; k++) {
                if (strlen(flags_table[k].flag) == keylen &&
                    strncmp(tok, flags_table[k].flag, keylen) == 0) {
                    if (!first) printf("\n");
                    printf("%s", tok);
                    first = 0;
                    break;
                }
            }
            continue;
        }
        for (int k = 0; flags_table[k].flag; k++) {
            if (strcmp(tok, flags_table[k].flag) == 0) {
                if (!first) printf("\n");
                printf("%s", tok);
                first = 0;
                if (flags_table[k].takes_value &&
                    i + 1 < nt && toks[i + 1][0] != '-') {
                    i++;
                    printf("\n%s", toks[i]);
                    break;
                }
                break;
            }
        }
    }
    for (int i = 0; i < nt; i++) free(toks[i]);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_relaunch_resolve_hermes_bin @ hermes_cli/relaunch.py:resolve_hermes_bin */
int hermes_cli_relaunch_resolve_hermes_bin(const char *arg) {
    /* Python: argv0 > PATH. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
