/* gate: port__hermes_cli_sqlite_safe_read_py___key — traced copy of sqlite_safe_key (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void sqlite_safe_key(const char *key, int delta);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sqlite_safe_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sqlite_safe_key");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    sqlite_safe_key(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sqlite_safe_key");
    }
    return 0;
}
