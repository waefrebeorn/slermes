/* gate: hermes_cli_gateway_py___spawn_gateway_restart_watcher — traced copy of gwc2_spawn_gateway_restart_watcher */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gwc2_spawn_gateway_restart_watcher(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("gwc2_spawn_gateway_restart_watcher");
    int result = gwc2_spawn_gateway_restart_watcher(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gwc2_spawn_gateway_restart_watcher");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwc2_spawn_gateway_restart_watcher <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
