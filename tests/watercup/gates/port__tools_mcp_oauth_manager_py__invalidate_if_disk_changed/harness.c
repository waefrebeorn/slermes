/* gate: {'name': 'port__tools_mcp_oauth_manager_py__invalidate_if_disk_changed', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '1', 'py_module': 'tools/mcp_oauth_manager.py', 'py_fn': 'invalidate_if_disk_changed', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/mcp_oauth_manager.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for invalidate_if_disk_changed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "invalidate_if_disk_changed";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s invalidate_if_disk_changed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("invalidate_if_disk_changed");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("invalidate_if_disk_changed");
    }
    return 0;
}
