/* gate: hermes_cli_gateway_py___runtime_dir_is_ours — traced copy of gw_runtime_dir_is_ours */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_runtime_dir_is_ours(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_runtime_dir_is_ours");
    bool result = gw_runtime_dir_is_ours(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_runtime_dir_is_ours");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_runtime_dir_is_ours <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
