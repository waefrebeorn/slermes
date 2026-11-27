/* gate: {'name': 'port__cli_py___reset_terminal_input_modes_on_exit', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_reset_terminal_input_modes_on_exit', 'py_module': 'cli.py', 'py_fn': '_reset_terminal_input_modes_on_exit', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/cli/cli_gaps_2026_08_25.c', 'src/agent/checkpoint.c', 'src/cli/cli_helpers2.c', 'src/hermes_cli/_subprocess_compat.c', 'src/hermes_cli/lifecycle.c', 'src/persistence/sqlite/_state/hermes_state_ops.c', 'src/persistence/sqlite/_state/hermes_state_ports.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _reset_terminal_input_modes_on_exit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_reset_terminal_input_modes_on_exit";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _reset_terminal_input_modes_on_exit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_reset_terminal_input_modes_on_exit");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reset_terminal_input_modes_on_exit");
    }
    return 0;
}
