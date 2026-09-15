/* gate: agent_pet_generate_imagegen_py___rejected_background — traced copy of agent_pet_generate_imagegen_u_rejected_background */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_pet_generate_imagegen_u_rejected_background(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_pet_generate_imagegen_u_rejected_background");
    int result = agent_pet_generate_imagegen_u_rejected_background(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_pet_generate_imagegen_u_rejected_background");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_pet_generate_imagegen_u_rejected_background <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
