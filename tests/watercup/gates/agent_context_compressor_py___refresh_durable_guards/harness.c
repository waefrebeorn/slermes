/* gate: agent_context_compressor_py___refresh_durable_guards — traced copy of ctxc_u_refresh_durable_guards */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ctxc_u_refresh_durable_guards(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ctxc_u_refresh_durable_guards");
    int result = ctxc_u_refresh_durable_guards(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ctxc_u_refresh_durable_guards");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ctxc_u_refresh_durable_guards <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
