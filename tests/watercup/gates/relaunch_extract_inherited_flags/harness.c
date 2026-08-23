/* gate: relaunch_extract_inherited_flags — traced copy of the FIXED
 * src/hermes_cli/ports_wrappers.c::hermes_cli_relaunch_u_extract_inherited_flags
 * (PoP: hermes_cli/relaunch.py:_extract_inherited_flags).
 * Input: one argv string of space-separated tokens.
 * Output V event: matched flags/values joined by \x1f.
 * PORT FIX (this batch): wrapper table was {"--tui","--web","--profile",
 * "--gateway"} all no-value with prefix matching; now mirrors LIVE's full
 * _INHERITED_FLAGS_TABLE with takes_value, exact matching, '=' form and
 * value consumption.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static const struct { const char *flag; int takes_value; } FLAGS[] = {
    {"-m", 1}, {"--model", 1}, {"--provider", 1}, {"--reasoning", 1},
    {"--accept-hooks", 0}, {"--skills", 1}, {"-s", 1}, {"--yolo", 0},
    {"--pass-session-id", 0}, {"--ignore-user-config", 0},
    {"--ignore-rules", 0}, {"--safe-mode", 0}, {"--tui", 0},
    {"--cli", 0}, {"--dev", 0}, {"--profile", 1}, {"-p", 1},
    {NULL, 0}
};

static void run_one(const char *arg) {
    char joined[8192]; size_t joff = 0; joined[0] = '\0';
    char *toks[256]; int nt = 0;
    WCUP_BEGIN("extract_inherited_flags");
    if (!arg || !*arg) { WCUP_BRANCH(1); }
    else {
        WCUP_BRANCH(0);
        const char *p = arg;
        while (*p && nt < 256) {
            const char *sp = strchr(p, ' ');
            size_t len = sp ? (size_t)(sp - p) : strlen(p);
            toks[nt++] = strndup(p, len);
            p = sp ? sp + 1 : p + len;
        }
        for (int i = 0; i < nt; i++) {
            WCUP_LOOP_TICK();
            const char *tok = toks[i];
            const char *eq = strchr(tok, '=');
            if (eq) {
                WCUP_BRANCH(1);
                size_t keylen = (size_t)(eq - tok);
                int hit = 0;
                for (int k = 0; FLAGS[k].flag; k++)
                    if (strlen(FLAGS[k].flag) == keylen &&
                        strncmp(tok, FLAGS[k].flag, keylen) == 0) { hit = 1; break; }
                if (hit) {
                    WCUP_BRANCH(1);
                    if (joff) joined[joff++] = '\x1f';
                    strcpy(joined + joff, tok); joff += strlen(tok);
                } else WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(0);
            int hit = -1;
            for (int k = 0; FLAGS[k].flag; k++)
                if (strcmp(tok, FLAGS[k].flag) == 0) { hit = k; break; }
            if (hit >= 0) {
                WCUP_BRANCH(1);
                if (joff) joined[joff++] = '\x1f';
                strcpy(joined + joff, tok); joff += strlen(tok);
                int take = FLAGS[hit].takes_value &&
                           i + 1 < nt && toks[i + 1][0] != '-';
                /* split into the three LIVE sub-rungs */
                if (FLAGS[hit].takes_value) {
                    WCUP_BRANCH(1);
                    if (i + 1 < nt) {
                        WCUP_BRANCH(1);
                        if (toks[i + 1][0] != '-') {
                            WCUP_BRANCH(1);
                            i++;
                            if (joff) joined[joff++] = '\x1f';
                            strcpy(joined + joff, toks[i]); joff += strlen(toks[i]);
                        } else WCUP_BRANCH(0);
                    } else WCUP_BRANCH(0);
                } else WCUP_BRANCH(0);
            } else WCUP_BRANCH(0);
        }
    }
    for (int i = 0; i < nt; i++) free(toks[i]);
    WCUP_VALSTR(joined);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("extract_inherited_flags");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract_inherited_flags <argv>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
