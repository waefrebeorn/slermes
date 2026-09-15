/* gate: tools_file_tools_py___file_ops_uses_host_paths — traced copy of filetools_uses_host_paths */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int filetools_uses_host_paths(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("filetools_uses_host_paths");
    int result = filetools_uses_host_paths(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("filetools_uses_host_paths");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s filetools_uses_host_paths <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
