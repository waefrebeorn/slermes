/* gate: {'name': 'port__cli_py__show_help', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': 'show_help', 'py_module': 'cli.py', 'py_fn': 'show_help', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/cli_cmd_skills.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for show_help */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "show_help";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s show_help <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("show_help");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("show_help");
    }
    return 0;
}
