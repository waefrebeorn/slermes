/* gate: hermes_cli_gateway_py__systemd_status — traced copy of cgw_systemd_status */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_systemd_status(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_systemd_status");
    int result = cgw_systemd_status(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_systemd_status");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_systemd_status <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
