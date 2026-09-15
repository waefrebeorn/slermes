/* gate: gateway_slash_commands_py___resume_row_visible — traced copy of slash_resume_row_visible */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool slash_resume_row_visible(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("slash_resume_row_visible");
    bool result = slash_resume_row_visible(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("slash_resume_row_visible");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s slash_resume_row_visible <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
