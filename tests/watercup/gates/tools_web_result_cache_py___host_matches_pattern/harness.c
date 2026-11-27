/* gate: {'name': 'tools_web_result_cache_py___host_matches_pattern', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_host_matches_pattern', 'py_module': 'tools/web_result_cache.py', 'py_fn': '_host_matches_pattern', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _host_matches_pattern */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_host_matches_pattern";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _host_matches_pattern <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_host_matches_pattern");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_host_matches_pattern");
    }
    return 0;
}
