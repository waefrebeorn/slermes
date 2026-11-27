/* gate: {'name': 'hermes_state_py___wal_generation_was_lost', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_wal_generation_was_lost', 'py_module': 'hermes_state.py', 'py_fn': '_wal_generation_was_lost', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _wal_generation_was_lost */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_wal_generation_was_lost";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _wal_generation_was_lost <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_wal_generation_was_lost");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_wal_generation_was_lost");
    }
    return 0;
}
