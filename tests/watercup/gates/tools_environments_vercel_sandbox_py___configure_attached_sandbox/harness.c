/* gate: tools_environments_vercel_sandbox_py___configure_attached_sandbox — traced copy of ve_env_configure_attached_sandbox */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ve_env_configure_attached_sandbox(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ve_env_configure_attached_sandbox");
    int result = ve_env_configure_attached_sandbox(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ve_env_configure_attached_sandbox");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ve_env_configure_attached_sandbox <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
