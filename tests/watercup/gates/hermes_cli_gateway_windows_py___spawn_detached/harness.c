/* gate: hermes_cli_gateway_windows_py___spawn_detached — traced copy of gw_u_spawn_detached */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_u_spawn_detached(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_u_spawn_detached");
    int result = gw_u_spawn_detached(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_u_spawn_detached");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_u_spawn_detached <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
