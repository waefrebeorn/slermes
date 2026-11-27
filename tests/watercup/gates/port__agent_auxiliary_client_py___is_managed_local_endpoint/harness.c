/* gate: {'name': 'port__agent_auxiliary_client_py___is_managed_local_endpoint', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_is_managed_local_endpoint', 'py_module': 'agent/auxiliary_client.py', 'py_fn': '_is_managed_local_endpoint', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/agent/auxiliary_client_gaps.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _is_managed_local_endpoint */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_is_managed_local_endpoint";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_managed_local_endpoint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_managed_local_endpoint");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_managed_local_endpoint");
    }
    return 0;
}
