/* gate: hermes_cli_plugins_py___VALID_PLUGIN_KINDS — traced copy of plugin_manifest_kind_is_valid */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool plugin_manifest_kind_is_valid(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("plugin_manifest_kind_is_valid");
    bool result = plugin_manifest_kind_is_valid(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("plugin_manifest_kind_is_valid");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s plugin_manifest_kind_is_valid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
