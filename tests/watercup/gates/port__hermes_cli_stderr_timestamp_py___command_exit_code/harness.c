/* gate: port__hermes_cli_stderr_timestamp_py___command_exit_code — traced copy of stderr_ts_command_exit_code (int_int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int stderr_ts_command_exit_code(int returncode);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stderr_ts_command_exit_code <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stderr_ts_command_exit_code");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = stderr_ts_command_exit_code((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stderr_ts_command_exit_code");
    }
    return 0;
}
