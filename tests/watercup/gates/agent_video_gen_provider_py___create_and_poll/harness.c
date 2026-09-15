/* gate: agent_video_gen_provider_py___create_and_poll — traced copy of agent_video_gen_provider_u_create_and_poll */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_video_gen_provider_u_create_and_poll(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_video_gen_provider_u_create_and_poll");
    int result = agent_video_gen_provider_u_create_and_poll(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_video_gen_provider_u_create_and_poll");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_video_gen_provider_u_create_and_poll <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
