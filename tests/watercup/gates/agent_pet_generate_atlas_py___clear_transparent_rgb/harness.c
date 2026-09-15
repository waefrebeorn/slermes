/* gate: agent_pet_generate_atlas_py___clear_transparent_rgb — traced copy of agent_pet_generate_atlas_u_clear_transparent_rgb */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_pet_generate_atlas_u_clear_transparent_rgb(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_pet_generate_atlas_u_clear_transparent_rgb");
    int result = agent_pet_generate_atlas_u_clear_transparent_rgb(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_pet_generate_atlas_u_clear_transparent_rgb");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_pet_generate_atlas_u_clear_transparent_rgb <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
