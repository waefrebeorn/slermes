/* gate: electron_main_cjs__createDir — traced copy of dir_create */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool dir_create(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dir_create");
    bool result = dir_create(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dir_create");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dir_create <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
