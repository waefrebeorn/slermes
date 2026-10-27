/* gate: port__hermes_cli_logs_py___follow_log — traced copy of cli_logs_follow (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void cli_logs_follow(const char *path, const char *min_level,
                     const char *session_filter, time_t since,
                     const char *comp_prefixes[], int ncomp);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_logs_follow <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_logs_follow");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_logs_follow();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_logs_follow");
    }
    return 0;
}
