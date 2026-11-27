/* gate: {'name': 'port__cli_py___cleanup_worktree', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_cleanup_worktree', 'py_module': 'cli.py', 'py_fn': '_cleanup_worktree', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/cli/cli_gaps_2026_08_25.c', 'src/agent/checkpoint.c', 'src/cli/cli_helpers2.c', 'src/hermes_cli/_subprocess_compat.c', 'src/hermes_cli/lifecycle.c', 'src/persistence/sqlite/_state/hermes_state_ops.c', 'src/persistence/sqlite/_state/hermes_state_ports.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src', '-DHERMES_VERSION="0.21.0"', '-DHERMES_RELEASE_DATE="2026-10-15"', '-DATADIR="/usr/share/hermes"'], 'classification': 'PORTED'} — GAP sentinel for _cleanup_worktree */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_cleanup_worktree";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _cleanup_worktree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_cleanup_worktree");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_cleanup_worktree");
    }
    return 0;
}
