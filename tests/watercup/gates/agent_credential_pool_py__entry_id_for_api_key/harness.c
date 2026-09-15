/* gate: agent_credential_pool_py__entry_id_for_api_key — traced copy of agent_credential_pool_entry_id_for_api_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_credential_pool_entry_id_for_api_key(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_credential_pool_entry_id_for_api_key");
    int result = agent_credential_pool_entry_id_for_api_key(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_credential_pool_entry_id_for_api_key");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_credential_pool_entry_id_for_api_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
