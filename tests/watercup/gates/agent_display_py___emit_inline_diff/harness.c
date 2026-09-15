/* gate: agent_display_py___emit_inline_diff — traced copy of disp_emit_inline_diff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool disp_emit_inline_diff(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("disp_emit_inline_diff");
    bool result = disp_emit_inline_diff(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("disp_emit_inline_diff");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_emit_inline_diff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
