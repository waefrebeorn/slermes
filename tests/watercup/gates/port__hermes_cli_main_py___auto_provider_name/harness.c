/* gate: {'name': 'port__hermes_cli_main_py___auto_provider_name', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_auto_provider_name', 'py_module': 'hermes_cli/main.py', 'py_fn': '_auto_provider_name', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/main_helpers.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src', '-DHERMES_VERSION="0.21.0"', '-DHERMES_RELEASE_DATE="2026-10-15"', '-DATADIR="/usr/share/hermes"'], 'classification': 'PORTED'} — GAP sentinel for _auto_provider_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_auto_provider_name";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _auto_provider_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_auto_provider_name");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_auto_provider_name");
    }
    return 0;
}
