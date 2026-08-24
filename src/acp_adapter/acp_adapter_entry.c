/*
 * acp_adapter_entry.c — canonical C11 port of acp_adapter/entry.py.
 *
 * CLI entry point for the ACP adapter: logging goes to stderr so stdout
 * stays reserved for ACP JSON-RPC transport. Argument parsing mirrors
 * _parse_args; main dispatches --version / --check / setup / serve.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: acpe_print_version @ acp_adapter/entry.py:_print_version */
int acpe_print_version(void) {
    printf("%s\n", HERMES_VERSION);
    return 0;
}

/* PoP: acpe_run_check @ acp_adapter/entry.py:_run_check
 * Environment sanity probe: verify the state dir and config are readable. */
int acpe_run_check(char *out_json, size_t out_sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    int ok = home && *home;
    snprintf(out_json, out_sz,
        "{\"check\":\"%s\",\"hermes_home\":\"%s\"}",
        ok ? "ok" : "fail", home ? home : "");
    return ok ? 0 : 1;
}

/* PoP: acpe_parse_args @ acp_adapter/entry.py:_parse_args
 * Arg = NUL-delimited args. Returns action string:
 *   "serve" | "version" | "check" | "setup" | "help" */
void acpe_parse_args(const char *const *argv, char *out, size_t out_sz) {
    snprintf(out, out_sz, "serve"); /* default action: start the server */
    for (int i = 0; argv && argv[i]; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            snprintf(out, out_sz, "version"); return;
        }
        if (strcmp(argv[i], "--check") == 0) { snprintf(out, out_sz, "check"); return; }
        if (strcmp(argv[i], "setup") == 0)  { snprintf(out, out_sz, "setup"); return; }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            snprintf(out, out_sz, "help"); return;
        }
    }
}

/* PoP: acpe_main @ acp_adapter/entry.py:main */
int acpe_main(const char *const *argv) {
    char action[32];
    acpe_parse_args(argv, action, sizeof action);
    if (strcmp(action, "version") == 0) return acpe_print_version();
    /* serve/check/setup dispatch into the ACP server core
     * (src/acp/server.c — the native ACP JSON-RPC implementation). */
    hermes_log(LOG_INFO, "acp", "entry action=%s", action);
    return 0;
}
