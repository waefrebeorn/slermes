/* gate: hermes_cli_plugins_cmd_py___git_pull_plugin_dir — traced copy of pcmd_git_pull_plugin_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pcmd_git_pull_plugin_dir(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pcmd_git_pull_plugin_dir");
    int result = pcmd_git_pull_plugin_dir(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pcmd_git_pull_plugin_dir");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pcmd_git_pull_plugin_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
