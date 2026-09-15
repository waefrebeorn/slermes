/* gate: gateway_slash_commands_py___resume_target_allowed — traced copy of slash_resume_target_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int slash_resume_target_allowed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("slash_resume_target_allowed");
    int result = slash_resume_target_allowed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("slash_resume_target_allowed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s slash_resume_target_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
