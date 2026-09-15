/* gate: agent_thread_scoped_output_py____getattr__ — traced copy of agent_thread_scoped_output_u__getattr__ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_thread_scoped_output_u__getattr__(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_thread_scoped_output_u__getattr__");
    int result = agent_thread_scoped_output_u__getattr__(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_thread_scoped_output_u__getattr__");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_thread_scoped_output_u__getattr__ <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
