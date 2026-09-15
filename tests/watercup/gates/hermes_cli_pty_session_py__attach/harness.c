/* gate: hermes_cli_pty_session_py__attach — traced copy of pty_attach */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pty_attach(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pty_attach");
    int result = pty_attach(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pty_attach");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pty_attach <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
