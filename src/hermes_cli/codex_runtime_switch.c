/*
 * codex_runtime_switch.c — canonical C11 port of hermes_cli/codex_runtime_switch.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_codex_runtime_switc_check_codex_binary_ok @ hermes_cli/codex_runtime_switch.py:check_codex_binary_ok */
int hermes_cli_codex_runtime_switc_check_codex_binary_ok(const char *arg) {
    /* Python: check_codex_binary() -> (ok, version_or_message). Arg =
     * "ok\tversion" (ok 1/0). */
    if (!arg || !*arg) { printf("0 codex check failed: not available\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%c %s\n", arg[0] == '1' ? '1' : '0',
           tab ? tab + 1 : (arg[0] == '1' ? "ok" : "failed"));
    return 0;
}
