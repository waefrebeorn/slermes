/* gate: {'name': 'port__tools_env_passthrough_py__clear_env_passthrough', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': 'allowlist', 'py_module': 'tools/env_passthrough.py', 'py_fn': 'clear_env_passthrough', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/tools/env_passthrough.c', 'src/agent/port_agent_secret_scope.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for clear_env_passthrough */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "clear_env_passthrough";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clear_env_passthrough <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("clear_env_passthrough");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("clear_env_passthrough");
    }
    return 0;
}
