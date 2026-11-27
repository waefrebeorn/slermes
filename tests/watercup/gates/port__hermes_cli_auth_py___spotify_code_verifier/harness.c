/* gate: {'name': 'port__hermes_cli_auth_py___spotify_code_verifier', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_spotify_code_verifier', 'py_module': 'hermes_cli/auth.py', 'py_fn': '_spotify_code_verifier', 'inputs': [0, 1, 42, 1024, -7], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/auth.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src', '-DHERMES_VERSION="0.21.0"', '-DHERMES_RELEASE_DATE="2026-10-15"', '-DATADIR="/usr/share/hermes"'], 'classification': 'PORTED'} — GAP sentinel for _spotify_code_verifier */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_spotify_code_verifier";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _spotify_code_verifier <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_spotify_code_verifier");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_spotify_code_verifier");
    }
    return 0;
}
