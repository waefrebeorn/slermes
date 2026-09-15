/* gate: hermes_cli_main_py___ensure_tui_workspace — traced copy of mn_ensure_tui_workspace */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mn_ensure_tui_workspace(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mn_ensure_tui_workspace");
    int result = mn_ensure_tui_workspace(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mn_ensure_tui_workspace");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_ensure_tui_workspace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
