/* gate: tools_browser_tool_py___should_inject_engine — traced copy of browser__should_inject_engine */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int browser__should_inject_engine(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("browser__should_inject_engine");
    int result = browser__should_inject_engine(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser__should_inject_engine");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser__should_inject_engine <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
