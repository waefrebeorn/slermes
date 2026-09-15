/* gate: hermes_cli_dashboard_auth_routes_py___reset_password_rate_limit — traced copy of hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit");
    int result = hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
