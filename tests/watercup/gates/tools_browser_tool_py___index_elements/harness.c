/* gate: tools_browser_tool_py___index_elements — traced copy of index_elements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int index_elements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("index_elements");
    int result = index_elements(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("index_elements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s index_elements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
