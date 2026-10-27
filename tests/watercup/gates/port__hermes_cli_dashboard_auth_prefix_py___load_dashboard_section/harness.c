/* gate: port__hermes_cli_dashboard_auth_prefix_py___load_dashboard_section — traced copy of cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section(char *public_url_out, size_t url_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section");
    }
    return 0;
}
