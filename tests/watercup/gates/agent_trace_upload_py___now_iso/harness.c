/* gate: agent_trace_upload_py___now_iso — traced copy of agent_trace_upload_u_now_iso */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_trace_upload_u_now_iso(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_trace_upload_u_now_iso");
    int result = agent_trace_upload_u_now_iso(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_trace_upload_u_now_iso");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_trace_upload_u_now_iso <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
