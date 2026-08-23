/* gate: gateway_status_gateway_command_subcommand — traced copy of
 * src/gateway/status.c::gateway_command_subcommand
 * (LIVE twin: gateway/status.py::_gateway_command_subcommand).
 *
 * Branch ladder (twin mirrors EXACTLY, same rung order):
 *   B1: empty command -> NULL
 *   B2: zero tokens after split -> NULL
 *   per raw token:
 *     B3: token is/ends-with gateway/run.py -> "run"
 *     B4: basename is hermes-gateway[.exe]  -> "run"
 *   B5: joined contains hermes_cli.main / hermes_cli/main.py
 *   B6: any token basename is hermes[.exe]
 *   B7: !has_entry -> NULL
 *   profile filter loop:
 *     B8: skip_next consumed
 *     B9: --profile/-p (set skip_next)
 *     B10: --profile=/-p= prefix dropped
 *   final loop:
 *     B11: token != gateway (continue)
 *     B12: i+1 >= fc -> "run"
 *   V1: result subcommand ("" for NULL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

/* faithful copies of status.c statics */
static void wc_norm_token(char *t) {
    for (char *p = t; *p; p++) {
        if (*p == '\\') *p = '/';
        else *p = (char)tolower((unsigned char)*p);
    }
}
static const char *wc_basename_tok(const char *t) {
    const char *slash = strrchr(t, '/');
    return slash ? slash + 1 : t;
}
static int wc_split_command(const char *command, char **argv, int max) {
    int argc = 0;
    const char *p = command;
    char buf[4096];
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        size_t bi = 0;
        char quote = 0;
        while (*p && bi < sizeof(buf) - 1) {
            char c = *p;
            if (quote) {
                if (c == quote) { quote = 0; p++; continue; }
                buf[bi++] = c; p++;
            } else if (c == '"' || c == '\'') {
                quote = c; p++;
            } else if (c == ' ' || c == '\t' || c == '\n') {
                break;
            } else {
                buf[bi++] = c; p++;
            }
        }
        buf[bi] = '\0';
        argv[argc++] = strdup(buf);
    }
    return argc;
}
static void wc_free_tokens(char **toks, int n) {
    for (int i = 0; i < n; i++) free(toks[i]);
}

static void wc_gateway_command_subcommand(const char *command) {
    WCUP_BEGIN("gateway_command_subcommand");
    const char *result = NULL;
    char resbuf[256]; resbuf[0] = '\0';
#define SET_RESULT(s) do { snprintf(resbuf, sizeof resbuf, "%s", (s)); } while (0)
    if (!command || !command[0]) {
        WCUP_BRANCH(1);                       /* B1 */
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char *raw[256];
        int rawc = wc_split_command(command, raw, 256);
        if (rawc == 0) { WCUP_BRANCH(1); wc_free_tokens(raw, rawc); goto out; } /* B2 */
        WCUP_BRANCH(0);
        for (int i = 0; i < rawc; i++) wc_norm_token(raw[i]);

        bool done = false;
        for (int i = 0; i < rawc && !done; i++) {
            const char *t = raw[i];
            size_t tl = strlen(t);
            const char *suffix = "/gateway/run.py";
            size_t sl = strlen(suffix);
            if (strcmp(t, "gateway/run.py") == 0 ||
                (tl >= sl && strcmp(t + tl - sl, suffix) == 0)) {
                WCUP_BRANCH(1);               /* B3 */
                SET_RESULT("run"); done = true;
            } else {
                WCUP_BRANCH(0);
                const char *bn = wc_basename_tok(t);
                if (strcmp(bn, "hermes-gateway") == 0 || strcmp(bn, "hermes-gateway.exe") == 0) {
                    WCUP_BRANCH(1);           /* B4 */
                    SET_RESULT("run"); done = true;
                } else {
                    WCUP_BRANCH(0);
                }
            }
        }

        bool has_entry = false;
        if (!done) {
            for (int i = 0; i < rawc && !has_entry; i++) {
                if (strstr(raw[i], "hermes_cli.main") || strstr(raw[i], "hermes_cli/main.py"))
                    has_entry = true;         /* B5 fires once */
            }
            if (!has_entry) {
                for (int i = 0; i < rawc; i++) {
                    const char *bn = wc_basename_tok(raw[i]);
                    if (strcmp(bn, "hermes") == 0 || strcmp(bn, "hermes.exe") == 0) {
                        has_entry = true; break;
                    }
                }
            }
            if (!has_entry) { WCUP_BRANCH(1); wc_free_tokens(raw, rawc); goto out; } /* B7 */
            WCUP_BRANCH(0);

            /* profile filter */
            char *filtered[256];
            int fc = 0;
            bool skip_next = false;
            for (int i = 0; i < rawc; i++) {
                const char *t = raw[i];
                if (skip_next) { skip_next = false; WCUP_BRANCH(1); continue; }  /* B8 */
                WCUP_BRANCH(0);
                if (strcmp(t, "--profile") == 0 || strcmp(t, "-p") == 0) {
                    skip_next = true; WCUP_BRANCH(1); continue;                  /* B9 */
                }
                WCUP_BRANCH(0);
                if (strncmp(t, "--profile=", 10) == 0 || strncmp(t, "-p=", 3) == 0) {
                    WCUP_BRANCH(1); continue;                                    /* B10 */
                }
                WCUP_BRANCH(0);
                filtered[fc++] = raw[i];
            }

            for (int i = 0; i < fc; i++) {
                if (strcmp(filtered[i], "gateway") != 0) { WCUP_BRANCH(1); continue; } /* B11 */
                WCUP_BRANCH(0);
                if (i + 1 >= fc) { WCUP_BRANCH(1); SET_RESULT("run"); break; }       /* B12 */
                WCUP_BRANCH(0);
                SET_RESULT(filtered[i + 1]);
                break;
            }
        } else {
            /* entrypoint short-circuit: twin emits no further rungs */
        }
        wc_free_tokens(raw, rawc);
    }
out:
    WCUP_VALSTR(resbuf);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gateway_command_subcommand");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_command_subcommand <cmdline>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_gateway_command_subcommand(argv[i]);
    return 0;
}
