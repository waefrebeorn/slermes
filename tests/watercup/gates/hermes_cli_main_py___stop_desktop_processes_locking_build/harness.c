/* gate: hermes_cli_main_py___stop_desktop_processes_locking_build — traced copy of main_u_stop_desktop_processes_locking_build */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_u_stop_desktop_processes_locking_build(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_u_stop_desktop_processes_locking_build");
    int result = main_u_stop_desktop_processes_locking_build(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_u_stop_desktop_processes_locking_build");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_u_stop_desktop_processes_locking_build <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
