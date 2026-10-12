/* gate: port__hermes_cli_stderr_timestamp_py___is_launchd_supervised — traced copy of stderr_ts_is_launchd_supervised (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int stderr_ts_is_launchd_supervised(const char *xpc_service_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stderr_ts_is_launchd_supervised <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stderr_ts_is_launchd_supervised");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = stderr_ts_is_launchd_supervised(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stderr_ts_is_launchd_supervised");
    }
    return 0;
}
