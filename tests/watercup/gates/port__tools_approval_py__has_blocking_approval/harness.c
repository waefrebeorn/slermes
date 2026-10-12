/* gate: port__tools_approval_py__has_blocking_approval — traced copy of apr2_has_blocking_approval (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool apr2_has_blocking_approval(const char *session_key);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_has_blocking_approval <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("apr2_has_blocking_approval");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = apr2_has_blocking_approval(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("apr2_has_blocking_approval");
    }
    return 0;
}
