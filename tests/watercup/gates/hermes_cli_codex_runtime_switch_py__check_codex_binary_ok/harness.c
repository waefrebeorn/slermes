/* gate: hermes_cli_codex_runtime_switch_py__check_codex_binary_ok
 *
 * Hand-written to match what the Python function actually returns. The generated
 * harness mixed only the int, so the twin (which mixed the whole (ok, message)
 * tuple) could never agree with it: the mismatch was a shape artefact, not a
 * divergence. Both faces are mixed here, in the same order as twin.py.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_codex_runtime_switc_check_codex_binary_ok(const char *input);
extern const char *hermes_cli_codex_runtime_switc_check_codex_binary_ok_msg(void);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_codex_runtime_switc_check_codex_binary_ok");
    int ok = hermes_cli_codex_runtime_switc_check_codex_binary_ok(input);
    WCUP_VALINT(ok);
    WCUP_VALSTR(hermes_cli_codex_runtime_switc_check_codex_binary_ok_msg());
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_codex_runtime_switc_check_codex_binary_ok");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s hermes_cli_codex_runtime_switc_check_codex_binary_ok <input>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
