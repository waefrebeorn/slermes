/* gate: hermes_cli_gateway_py__generate_systemd_unit — traced copy of cgw_generate_systemd_unit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_generate_systemd_unit(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_generate_systemd_unit");
    int result = cgw_generate_systemd_unit(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_generate_systemd_unit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_generate_systemd_unit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
