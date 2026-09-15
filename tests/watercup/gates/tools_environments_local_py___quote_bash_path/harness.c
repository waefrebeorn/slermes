/* gate: tools_environments_local_py___quote_bash_path — traced copy of envl_u_quote_bash_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envl_u_quote_bash_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envl_u_quote_bash_path");
    int result = envl_u_quote_bash_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envl_u_quote_bash_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envl_u_quote_bash_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
