/* gate: gateway_run_py___handle_message_with_agent — traced copy of grun_u_handle_message_with_agent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_handle_message_with_agent(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_handle_message_with_agent");
    int result = grun_u_handle_message_with_agent(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_handle_message_with_agent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_handle_message_with_agent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
