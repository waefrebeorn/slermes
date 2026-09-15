/* gate: hermes_cli_web_server_py___validate_plugin_name — traced copy of web_validate_plugin_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool web_validate_plugin_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("web_validate_plugin_name");
    bool result = web_validate_plugin_name(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("web_validate_plugin_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s web_validate_plugin_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
