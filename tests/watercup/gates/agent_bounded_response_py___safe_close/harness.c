/* gate: agent_bounded_response_py___safe_close — traced copy of agent_bounded_response_u_safe_close */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_bounded_response_u_safe_close(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_bounded_response_u_safe_close");
    int result = agent_bounded_response_u_safe_close(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_bounded_response_u_safe_close");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_bounded_response_u_safe_close <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
