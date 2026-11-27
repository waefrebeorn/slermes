/* gate: {'name': 'port__tools_fuzzy_match_py___detect_backslash_doubling', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_detect_backslash_doubling', 'py_module': 'tools/fuzzy_match.py', 'py_fn': '_detect_backslash_doubling', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/tools/fuzzy_match_gaps3.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _detect_backslash_doubling */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_detect_backslash_doubling";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _detect_backslash_doubling <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_detect_backslash_doubling");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_detect_backslash_doubling");
    }
    return 0;
}
