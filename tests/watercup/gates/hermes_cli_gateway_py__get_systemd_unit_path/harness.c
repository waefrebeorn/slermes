/* gate: hermes_cli_gateway_py__get_systemd_unit_path — traced copy of cgw_get_systemd_unit_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_get_systemd_unit_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_get_systemd_unit_path");
    int result = cgw_get_systemd_unit_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_get_systemd_unit_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_get_systemd_unit_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
