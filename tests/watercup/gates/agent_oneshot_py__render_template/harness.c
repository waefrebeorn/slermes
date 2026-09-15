/* gate: agent_oneshot_py__render_template — traced copy of agent_oneshot_render_template */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_oneshot_render_template(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_oneshot_render_template");
    int result = agent_oneshot_render_template(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_oneshot_render_template");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_oneshot_render_template <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
