/* gate: port__hermes_cli_dashboard_auth_prefix_py___warn_if_malformed_prefix — traced copy of hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix");
    }
    return 0;
}
