/* gate: agent_model_metadata_py___estimate_message_tokens_without_images — traced copy of agent_model_metadata_u_estimate_message_tokens_without_images */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_model_metadata_u_estimate_message_tokens_without_images(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_model_metadata_u_estimate_message_tokens_without_images");
    int result = agent_model_metadata_u_estimate_message_tokens_without_images(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_model_metadata_u_estimate_message_tokens_without_images");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_model_metadata_u_estimate_message_tokens_without_images <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
