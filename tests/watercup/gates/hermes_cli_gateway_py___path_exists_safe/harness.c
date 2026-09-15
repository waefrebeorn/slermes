/* gate: hermes_cli_gateway_py___path_exists_safe — traced copy of gw_path_exists_safe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_path_exists_safe(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_path_exists_safe");
    bool result = gw_path_exists_safe(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_path_exists_safe");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_path_exists_safe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
