/* gate: port__tools_approval_py___has_allowlist_shell_operator — traced copy of apr_has_allowlist_shell_operator (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool apr_has_allowlist_shell_operator(const char *command);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr_has_allowlist_shell_operator <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("apr_has_allowlist_shell_operator");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = apr_has_allowlist_shell_operator(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("apr_has_allowlist_shell_operator");
    }
    return 0;
}
