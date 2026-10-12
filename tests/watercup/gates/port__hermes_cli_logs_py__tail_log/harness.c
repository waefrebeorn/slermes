/* gate: port__hermes_cli_logs_py__tail_log — traced copy of cli_logs_tail (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void cli_logs_tail(const char *log_name, int num_lines, int follow,
                   const char *level, const char *session,
                   const char *since, const char *component);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_logs_tail <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_logs_tail");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_logs_tail(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5], argv[2 + (i - 2 + 6) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_logs_tail");
    }
    return 0;
}
