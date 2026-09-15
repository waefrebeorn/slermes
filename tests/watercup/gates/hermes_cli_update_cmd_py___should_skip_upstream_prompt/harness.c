/* gate: hermes_cli_update_cmd_py___should_skip_upstream_prompt — traced copy of uc_should_skip_upstream_prompt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool uc_should_skip_upstream_prompt(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uc_should_skip_upstream_prompt");
    bool result = uc_should_skip_upstream_prompt(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_should_skip_upstream_prompt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_should_skip_upstream_prompt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
