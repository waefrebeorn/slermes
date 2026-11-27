/* gate: {'name': 'tools_file_operations_py___has_ancestor_tsconfig', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_has_ancestor_tsconfig', 'py_module': 'tools/file_operations.py', 'py_fn': '_has_ancestor_tsconfig', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _has_ancestor_tsconfig */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_has_ancestor_tsconfig";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _has_ancestor_tsconfig <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_has_ancestor_tsconfig");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_has_ancestor_tsconfig");
    }
    return 0;
}
