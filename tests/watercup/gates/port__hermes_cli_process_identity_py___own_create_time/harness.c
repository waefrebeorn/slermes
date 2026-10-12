/* gate: port__hermes_cli_process_identity_py___own_create_time — traced copy of process_identity_own_create_time (double_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern double process_identity_own_create_time(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s process_identity_own_create_time <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("process_identity_own_create_time");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = process_identity_own_create_time(strtod(argv[i], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("process_identity_own_create_time");
    }
    return 0;
}
