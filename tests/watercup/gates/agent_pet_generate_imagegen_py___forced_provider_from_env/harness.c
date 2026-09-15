/* gate: agent_pet_generate_imagegen_py___forced_provider_from_env — traced copy of agent_pet_generate_imagegen_u_forced_provider_from_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_pet_generate_imagegen_u_forced_provider_from_env(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_pet_generate_imagegen_u_forced_provider_from_env");
    int result = agent_pet_generate_imagegen_u_forced_provider_from_env(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_pet_generate_imagegen_u_forced_provider_from_env");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_pet_generate_imagegen_u_forced_provider_from_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
