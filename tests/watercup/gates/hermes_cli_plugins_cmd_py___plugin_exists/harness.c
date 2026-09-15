/* gate: hermes_cli_plugins_cmd_py___plugin_exists — traced copy of pcmd_plugin_exists */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pcmd_plugin_exists(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pcmd_plugin_exists");
    int result = pcmd_plugin_exists(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pcmd_plugin_exists");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pcmd_plugin_exists <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
