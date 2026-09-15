/* gate: hermes_cli_plugins_cmd_py___require_installed_plugin — traced copy of pcmd_require_installed_plugin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pcmd_require_installed_plugin(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("pcmd_require_installed_plugin");
    int result = pcmd_require_installed_plugin(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pcmd_require_installed_plugin");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pcmd_require_installed_plugin <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
