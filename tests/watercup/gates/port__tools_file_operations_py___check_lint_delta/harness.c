/* gate: port__tools_file_operations_py___check_lint_delta — traced copy of fo_check_lint_delta (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool fo_check_lint_delta(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_check_lint_delta <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fo_check_lint_delta");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = fo_check_lint_delta();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fo_check_lint_delta");
    }
    return 0;
}
