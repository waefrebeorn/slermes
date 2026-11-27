/* gate: {'name': 'port__hermes_cli_auth_py__is_source_suppressed', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': 'JSON', 'py_module': 'hermes_cli/auth.py', 'py_fn': 'is_source_suppressed', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/auth.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for is_source_suppressed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "is_source_suppressed";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_source_suppressed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("is_source_suppressed");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_source_suppressed");
    }
    return 0;
}
