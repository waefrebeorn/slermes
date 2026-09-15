/* gate: hermes_cli_windows_ssh_runtime_py__terminate_owned — traced copy of wssr_terminate_owned */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wssr_terminate_owned(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wssr_terminate_owned");
    int result = wssr_terminate_owned(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wssr_terminate_owned");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wssr_terminate_owned <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
