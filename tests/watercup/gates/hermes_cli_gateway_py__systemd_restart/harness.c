/* gate: hermes_cli_gateway_py__systemd_restart — traced copy of cgw_systemd_restart */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_systemd_restart();

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("cgw_systemd_restart");
    int result = cgw_systemd_restart();
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_systemd_restart");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_systemd_restart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
