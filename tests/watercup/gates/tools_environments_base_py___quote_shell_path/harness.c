/* gate: tools_environments_base_py___quote_shell_path — traced copy of envb_u_quote_shell_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envb_u_quote_shell_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envb_u_quote_shell_path");
    int result = envb_u_quote_shell_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envb_u_quote_shell_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_u_quote_shell_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
