/* gate: agent_pet_store_py__exists — traced copy of file_exists */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool file_exists(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("file_exists");
    bool result = file_exists(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_exists");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_exists <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
