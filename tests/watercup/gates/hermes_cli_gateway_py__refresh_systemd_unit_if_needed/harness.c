/* gate: hermes_cli_gateway_py__refresh_systemd_unit_if_needed — traced copy of cgw_refresh_systemd_unit_if_needed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_refresh_systemd_unit_if_needed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_refresh_systemd_unit_if_needed");
    int result = cgw_refresh_systemd_unit_if_needed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_refresh_systemd_unit_if_needed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_refresh_systemd_unit_if_needed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
