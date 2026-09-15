/* gate: tools_environments_local_py___prepend_shell_init — traced copy of envl_u_prepend_shell_init */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envl_u_prepend_shell_init(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envl_u_prepend_shell_init");
    int result = envl_u_prepend_shell_init(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envl_u_prepend_shell_init");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envl_u_prepend_shell_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
