/* gate: {'name': 'port__hermes_cli_kanban_py___cmd_tail', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_cmd_tail', 'py_module': 'hermes_cli/kanban.py', 'py_fn': '_cmd_tail', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/kanban_cmd_gaps.c', 'src/hermes_cli/kanban_specify.c', 'src/hermes_cli/kanban_format.c', 'src/hermes_cli/kanban_diagnostics.c', 'src/hermes_cli/kanban_helpers.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _cmd_tail */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_cmd_tail";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _cmd_tail <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_cmd_tail");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_cmd_tail");
    }
    return 0;
}
