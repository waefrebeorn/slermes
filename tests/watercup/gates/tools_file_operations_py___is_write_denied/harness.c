/* gate: tools_file_operations_py___is_write_denied — traced copy of fo_is_write_denied */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool fo_is_write_denied(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fo_is_write_denied");
    bool result = fo_is_write_denied(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_is_write_denied");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_is_write_denied <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
