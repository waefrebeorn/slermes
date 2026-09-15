/* gate: hermes_cli_gateway_py__gateway_command — traced copy of gateway_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_command");
    int result = gateway_command(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
