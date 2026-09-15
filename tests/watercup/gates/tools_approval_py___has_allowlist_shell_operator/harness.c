/* gate: tools_approval_py___has_allowlist_shell_operator — traced copy of apr_has_allowlist_shell_operator */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool apr_has_allowlist_shell_operator(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apr_has_allowlist_shell_operator");
    bool result = apr_has_allowlist_shell_operator(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr_has_allowlist_shell_operator");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr_has_allowlist_shell_operator <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
