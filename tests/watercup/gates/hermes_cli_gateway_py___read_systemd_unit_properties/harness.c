/* gate: hermes_cli_gateway_py___read_systemd_unit_properties — traced copy of cgw_u_read_systemd_unit_properties */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_read_systemd_unit_properties(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_read_systemd_unit_properties");
    int result = cgw_u_read_systemd_unit_properties(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_read_systemd_unit_properties");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_read_systemd_unit_properties <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
