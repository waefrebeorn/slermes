/* gate: hermes_cli_gateway_py___raise_user_systemd_unavailable — traced copy of cgw_u_raise_user_systemd_unavailable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_raise_user_systemd_unavailable(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_raise_user_systemd_unavailable");
    int result = cgw_u_raise_user_systemd_unavailable(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_raise_user_systemd_unavailable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_raise_user_systemd_unavailable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
