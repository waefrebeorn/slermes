/* gate: hermes_cli_gateway_py___windows_scheduled_task_running — traced copy of _windows_scheduled_task_running */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _windows_scheduled_task_running(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_windows_scheduled_task_running");
    int result = _windows_scheduled_task_running(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_windows_scheduled_task_running");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _windows_scheduled_task_running <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
