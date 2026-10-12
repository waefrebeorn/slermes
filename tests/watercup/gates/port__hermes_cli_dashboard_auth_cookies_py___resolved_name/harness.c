/* gate: port__hermes_cli_dashboard_auth_cookies_py___resolved_name — traced copy of dash_resolved_name (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void dash_resolved_name(const char *bare, int use_https, const char *prefix,
                         char *out, size_t out_cap);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dash_resolved_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dash_resolved_name");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    dash_resolved_name(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dash_resolved_name");
    }
    return 0;
}
