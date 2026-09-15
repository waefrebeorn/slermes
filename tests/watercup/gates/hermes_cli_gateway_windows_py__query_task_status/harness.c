/* gate: hermes_cli_gateway_windows_py__query_task_status — traced copy of gw_query_task_status */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_query_task_status(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_query_task_status");
    int result = gw_query_task_status(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_query_task_status");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_query_task_status <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
