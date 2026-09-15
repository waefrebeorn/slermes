/* gate: agent_auxiliary_client_py___obj_get — traced copy of agent_auxiliary_client_u_obj_get */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_auxiliary_client_u_obj_get(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_auxiliary_client_u_obj_get");
    int result = agent_auxiliary_client_u_obj_get(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_auxiliary_client_u_obj_get");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_auxiliary_client_u_obj_get <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
