/* gate: port__hermes_cli_auth_py___oauth_trace_enabled — traced copy of auth_oauth_trace_enabled (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool auth_oauth_trace_enabled(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_oauth_trace_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_oauth_trace_enabled");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = auth_oauth_trace_enabled();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_oauth_trace_enabled");
    }
    return 0;
}
