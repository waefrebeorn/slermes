/* gate: hermes_cli_tools_config_py___post_setup_already_installed — traced copy of tools_config_post_setup_already_installed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tools_config_post_setup_already_installed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_config_post_setup_already_installed");
    bool result = tools_config_post_setup_already_installed(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_config_post_setup_already_installed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_config_post_setup_already_installed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
