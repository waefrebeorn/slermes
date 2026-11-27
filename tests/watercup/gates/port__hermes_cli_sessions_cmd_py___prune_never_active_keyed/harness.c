/* gate: {'name': 'port__hermes_cli_sessions_cmd_py___prune_never_active_keyed', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_prune_never_active_keyed', 'py_module': 'hermes_cli/sessions_cmd.py', 'py_fn': '_prune_never_active_keyed', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/sessions_cmd_gaps3.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _prune_never_active_keyed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_prune_never_active_keyed";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prune_never_active_keyed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prune_never_active_keyed");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prune_never_active_keyed");
    }
    return 0;
}
