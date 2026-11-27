/* gate: {'name': 'cli_py___should_emit_cleanup_session_finalize', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_should_emit_cleanup_session_finalize', 'py_module': 'cli.py', 'py_fn': '_should_emit_cleanup_session_finalize', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _should_emit_cleanup_session_finalize */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_should_emit_cleanup_session_finalize";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _should_emit_cleanup_session_finalize <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_should_emit_cleanup_session_finalize");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_should_emit_cleanup_session_finalize");
    }
    return 0;
}
