/* gate: hermes_cli_windows_ssh_runtime_py___current_sid — traced copy of wssr_u_current_sid */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wssr_u_current_sid(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wssr_u_current_sid");
    int result = wssr_u_current_sid(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wssr_u_current_sid");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wssr_u_current_sid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
