/* gate: approval_read_tool_exec_flag — traced C copy of
 * src/tools/approval_wrappers.c::appr_u_read_tool_exec_flag.
 * LIVE twin: tools/approval.py::_read_tool_exec_flag (called for the result;
 * the twin mirrors the same token ladder using the LIVE flag tables).
 *
 * The C shim prints its result internally, so a traced copy carries the value
 * into the cup instead of stdout (one cup line per input, argv-safe).
 *
 * Shared ladder per token (must match twin.py EXACTLY, same rung order):
 *   LOOP tick
 *   B1: token == "--"            -> stop scanning
 *   B2: man -P/-H attached payload special case
 *   B3: option matches the tool's exec-flag set
 *   B4: payload present and non-empty -> result
 *   V : "option\tpayload" or "" when no flag found
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void trace_one(const char *tool, size_t tlen, char *args) {
    WCUP_BEGIN("read_tool_exec_flag");
    int is_man = (tlen == 3 && strncmp(tool, "man", 3) == 0);
    char *save = NULL;
    char *toks[64];
    int ntok = 0;
    for (char *tok = strtok_r(args, " ", &save); tok && ntok < 64;
         tok = strtok_r(NULL, " ", &save))
        toks[ntok++] = tok;

    const char *result_opt = NULL, *result_payload = NULL;
    char ropt[128], rpay[128];
    for (int i = 0; i < ntok; i++) {
        WCUP_LOOP_TICK();
        char *tok = toks[i];
        int is_sep = strcmp(tok, "--") == 0;
        WCUP_BRANCH(is_sep);                                   /* B1 */
        if (is_sep) break;

        char *eq = strchr(tok, '=');
        char opt[128];
        const char *payload = NULL;
        if (eq) {
            size_t ol = (size_t)(eq - tok);
            if (ol >= sizeof(opt)) ol = sizeof(opt) - 1;
            memcpy(opt, tok, ol);
            opt[ol] = '\0';
            payload = eq + 1;
        } else {
            snprintf(opt, sizeof(opt), "%s", tok);
        }

        int man_attached = (is_man &&
                            (strncmp(tok, "-P", 2) == 0 ||
                             strncmp(tok, "-H", 2) == 0) &&
                            strlen(tok) > 2);
        WCUP_BRANCH(man_attached);                             /* B2 */
        int matched = man_attached;
        if (man_attached) {
            snprintf(opt, sizeof(opt), "%.2s", tok);
            payload = tok + 2;
        } else if (tlen == 4 && strncmp(tool, "sort", 4) == 0 &&
                   strcmp(opt, "--compress-program") == 0) matched = 1;
        else if (tlen == 2 && strncmp(tool, "rg", 2) == 0 &&
                 (strcmp(opt, "--pre") == 0 ||
                  strcmp(opt, "--hostname-bin") == 0)) matched = 1;
        else if (tlen == 2 && strncmp(tool, "ag", 2) == 0 &&
                 strcmp(opt, "--pager") == 0) matched = 1;
        else if (is_man && (strcmp(opt, "--pager") == 0 ||
                            strcmp(opt, "--html") == 0 ||
                            strcmp(opt, "-P") == 0 ||
                            strcmp(opt, "-H") == 0)) matched = 1;
        WCUP_BRANCH(matched);                                  /* B3 */
        if (matched) {
            if (!payload && i + 1 < ntok) payload = toks[i + 1];
            int have = payload && *payload;
            WCUP_BRANCH(have);                                 /* B4 */
            if (have) {
                snprintf(ropt, sizeof ropt, "%s", opt);
                snprintf(rpay, sizeof rpay, "%s", payload);
                result_opt = ropt; result_payload = rpay;
                break;
            }
        } else {
            WCUP_BRANCH(0);                                    /* B4 not reached */
        }
    }

    char res[256];
    if (result_opt) snprintf(res, sizeof res, "%s\t%s", result_opt, result_payload);
    else res[0] = '\0';
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("read_tool_exec_flag");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s read_tool_exec_flag <tool args...>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *pair = strdup(argv[i]);
        char *sp = strchr(pair, ' ');
        if (!sp) { fprintf(stderr, "bad input %s\n", argv[i]); free(pair); return 2; }
        *sp = '\0';
        trace_one(pair, strlen(pair), sp + 1);
        free(pair);
    }
    return 0;
}
