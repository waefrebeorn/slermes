/* gate: agent_display_py___result_succeeded — traced copy of disp_result_succeeded */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool disp_result_succeeded(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("disp_result_succeeded");
    bool result = disp_result_succeeded(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("disp_result_succeeded");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_result_succeeded <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
