/* gate: port__hermes_cli_web_server_py__should_require_dashboard_auth — traced copy of should_require_dashboard_auth (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int should_require_dashboard_auth(const char *host);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s should_require_dashboard_auth <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("should_require_dashboard_auth");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = should_require_dashboard_auth(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("should_require_dashboard_auth");
    }
    return 0;
}
