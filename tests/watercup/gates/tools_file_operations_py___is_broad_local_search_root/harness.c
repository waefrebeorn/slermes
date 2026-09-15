/* gate: tools_file_operations_py___is_broad_local_search_root — traced copy of _is_broad_local_search_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _is_broad_local_search_root(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_broad_local_search_root");
    int result = _is_broad_local_search_root(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_broad_local_search_root");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_broad_local_search_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
