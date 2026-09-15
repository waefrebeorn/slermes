/* gate: hermes_cli_plugins_py__remove_plugin_skill — traced copy of plug_remove_skill */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool plug_remove_skill(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("plug_remove_skill");
    bool result = plug_remove_skill(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("plug_remove_skill");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s plug_remove_skill <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
