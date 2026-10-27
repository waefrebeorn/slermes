/* gate: port__cli_py___deepen_shallow_repo — traced copy of _deepen_shallow_repo (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int _deepen_shallow_repo(const char *repo_root, int timeout);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _deepen_shallow_repo <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_deepen_shallow_repo");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _deepen_shallow_repo(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_deepen_shallow_repo");
    }
    return 0;
}
