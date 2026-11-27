/* gate: {'name': 'port__cli_py___arm_exit_watchdog', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_arm_exit_watchdog', 'py_module': 'cli.py', 'py_fn': '_arm_exit_watchdog', 'inputs': [0.0, 1.5, 3.14159, 1024.0, -2.5], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/cli/cli_gaps_2026_08_25.c', 'src/agent/checkpoint.c', 'src/cli/cli_helpers2.c', 'src/hermes_cli/_subprocess_compat.c', 'src/hermes_cli/lifecycle.c', 'src/persistence/sqlite/_state/hermes_state_ops.c', 'src/persistence/sqlite/_state/hermes_state_ports.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _arm_exit_watchdog */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_arm_exit_watchdog";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _arm_exit_watchdog <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_arm_exit_watchdog");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_arm_exit_watchdog");
    }
    return 0;
}
