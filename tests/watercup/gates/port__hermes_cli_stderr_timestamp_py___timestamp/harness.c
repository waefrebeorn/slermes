/* gate: port__hermes_cli_stderr_timestamp_py___timestamp — traced copy of stderr_ts_timestamp (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void stderr_ts_timestamp(char *buf, size_t cap);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stderr_ts_timestamp <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stderr_ts_timestamp");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    stderr_ts_timestamp(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stderr_ts_timestamp");
    }
    return 0;
}
