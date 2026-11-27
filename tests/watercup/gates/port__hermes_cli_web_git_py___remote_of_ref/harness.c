/* gate: {'name': 'port__hermes_cli_web_git_py___remote_of_ref', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_remote_of_ref', 'py_module': 'hermes_cli/web_git.py', 'py_fn': '_remote_of_ref', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/web_git_gaps3.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _remote_of_ref */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_remote_of_ref";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _remote_of_ref <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_remote_of_ref");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_remote_of_ref");
    }
    return 0;
}
