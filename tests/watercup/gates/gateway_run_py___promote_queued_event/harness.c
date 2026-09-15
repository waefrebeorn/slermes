/* gate: gateway_run_py___promote_queued_event — traced copy of grun_u_promote_queued_event */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_promote_queued_event(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_promote_queued_event");
    int result = grun_u_promote_queued_event(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_promote_queued_event");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_promote_queued_event <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
