/* gate: tools_browser_supervisor_py___run — traced copy of browser_supervisor_run */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int browser_supervisor_run(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("browser_supervisor_run");
    int result = browser_supervisor_run(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser_supervisor_run");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_supervisor_run <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
