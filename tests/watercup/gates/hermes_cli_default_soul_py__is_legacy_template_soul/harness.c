/* gate: hermes_cli_default_soul_py__is_legacy_template_soul — traced copy of hermes_cli_default_soul_is_legacy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hermes_cli_default_soul_is_legacy(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_default_soul_is_legacy");
    bool result = hermes_cli_default_soul_is_legacy(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_default_soul_is_legacy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_default_soul_is_legacy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
