/* gate: hermes_cli_gateway_py__print_systemd_linger_guidance — traced copy of cgw_print_systemd_linger_guidance */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_print_systemd_linger_guidance(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_print_systemd_linger_guidance");
    int result = cgw_print_systemd_linger_guidance(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_print_systemd_linger_guidance");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_print_systemd_linger_guidance <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
