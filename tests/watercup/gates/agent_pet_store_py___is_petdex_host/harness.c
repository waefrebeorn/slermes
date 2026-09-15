/* gate: agent_pet_store_py___is_petdex_host — traced copy of pet_is_petdex_host */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool pet_is_petdex_host(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pet_is_petdex_host");
    bool result = pet_is_petdex_host(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pet_is_petdex_host");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pet_is_petdex_host <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
