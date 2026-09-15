/* gate: agent_display_py___write — traced copy of disp_spinner_write */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int disp_spinner_write(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("disp_spinner_write");
    int result = disp_spinner_write(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("disp_spinner_write");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_spinner_write <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
