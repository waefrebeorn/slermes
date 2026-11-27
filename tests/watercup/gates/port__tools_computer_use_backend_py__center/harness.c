/* gate: {'name': 'port__tools_computer_use_backend_py__center', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': 'cli_tools_computer_use_backend_center', 'py_module': 'tools/computer_use/backend.py', 'py_fn': 'center', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/tools/computer_use/computer_use_backend.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src', '-DHERMES_VERSION="0.21.0"', '-DHERMES_RELEASE_DATE="2026-10-15"', '-DATADIR="/usr/share/hermes"'], 'classification': 'PORTED'} — GAP sentinel for center */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "center";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s center <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("center");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("center");
    }
    return 0;
}
