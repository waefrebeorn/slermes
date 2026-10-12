/* gate: port__hermes_cli_browser_connect_py___secure_snapshot_contents — traced copy of _secure_snapshot_contents (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _secure_snapshot_contents(const char *src, const char *dst);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _secure_snapshot_contents <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_secure_snapshot_contents");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _secure_snapshot_contents(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_secure_snapshot_contents");
    }
    return 0;
}
