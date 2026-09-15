/* gate: hermes_cli_uninstall_py__run_gui_uninstall — traced copy of uninst_run_gui_uninstall */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uninst_run_gui_uninstall(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uninst_run_gui_uninstall");
    int result = uninst_run_gui_uninstall(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uninst_run_gui_uninstall");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uninst_run_gui_uninstall <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
