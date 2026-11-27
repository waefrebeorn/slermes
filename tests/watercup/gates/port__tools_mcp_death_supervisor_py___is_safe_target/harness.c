/* gate: {'name': 'port__tools_mcp_death_supervisor_py___is_safe_target', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_is_safe_target', 'py_module': 'tools/mcp_death_supervisor.py', 'py_fn': '_is_safe_target', 'inputs': [0, 1, 42, 1024, -7], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/tools/mcp_death_supervisor_gaps3.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _is_safe_target */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_is_safe_target";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_safe_target <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_safe_target");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_safe_target");
    }
    return 0;
}
