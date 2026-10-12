/* gate: port__tools_file_operations_py___has_ancestor_tsconfig — traced copy of _has_ancestor_tsconfig (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _has_ancestor_tsconfig(const char *path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _has_ancestor_tsconfig <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_has_ancestor_tsconfig");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _has_ancestor_tsconfig(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_has_ancestor_tsconfig");
    }
    return 0;
}
