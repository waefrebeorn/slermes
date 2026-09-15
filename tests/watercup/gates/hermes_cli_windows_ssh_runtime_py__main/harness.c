/* gate: hermes_cli_windows_ssh_runtime_py__main — traced copy of wss_main */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wss_main(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wss_main");
    int result = wss_main(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wss_main");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wss_main <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
