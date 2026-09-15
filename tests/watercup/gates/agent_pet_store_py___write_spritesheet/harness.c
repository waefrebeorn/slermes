/* gate: agent_pet_store_py___write_spritesheet — traced copy of pet_write_spritesheet */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pet_write_spritesheet(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pet_write_spritesheet");
    int result = pet_write_spritesheet(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pet_write_spritesheet");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pet_write_spritesheet <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
