/* gate: port__hermes_cli_dashboard_auth_cookies_py__clear_pkce_cookie — traced copy of dash_clear_pkce_cookie (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int dash_clear_pkce_cookie(char *out[], int out_cap, const char *prefix);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dash_clear_pkce_cookie <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dash_clear_pkce_cookie");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = dash_clear_pkce_cookie((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dash_clear_pkce_cookie");
    }
    return 0;
}
