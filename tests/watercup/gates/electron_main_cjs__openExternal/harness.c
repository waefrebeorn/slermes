/* gate: electron_main_cjs__openExternal — traced copy of desktop_open_external */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool desktop_open_external(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("desktop_open_external");
    bool result = desktop_open_external(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("desktop_open_external");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s desktop_open_external <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
