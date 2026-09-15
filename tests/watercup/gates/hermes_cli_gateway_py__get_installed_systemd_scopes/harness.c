/* gate: hermes_cli_gateway_py__get_installed_systemd_scopes — traced copy of cgw_get_installed_systemd_scopes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_get_installed_systemd_scopes(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_get_installed_systemd_scopes");
    int result = cgw_get_installed_systemd_scopes(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_get_installed_systemd_scopes");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_get_installed_systemd_scopes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
