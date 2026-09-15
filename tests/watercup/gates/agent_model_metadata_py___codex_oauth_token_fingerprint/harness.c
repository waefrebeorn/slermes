/* gate: agent_model_metadata_py___codex_oauth_token_fingerprint — traced copy of agent_model_metadata_u_codex_oauth_token_fingerprint */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_model_metadata_u_codex_oauth_token_fingerprint(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_model_metadata_u_codex_oauth_token_fingerprint");
    int result = agent_model_metadata_u_codex_oauth_token_fingerprint(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_model_metadata_u_codex_oauth_token_fingerprint");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_model_metadata_u_codex_oauth_token_fingerprint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
