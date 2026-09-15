/* gate: hermes_cli_tools_config_py___run_post_setup — traced copy of tools_config_run_post_setup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_config_run_post_setup(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_config_run_post_setup");
    int result = tools_config_run_post_setup(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_config_run_post_setup");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_config_run_post_setup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
