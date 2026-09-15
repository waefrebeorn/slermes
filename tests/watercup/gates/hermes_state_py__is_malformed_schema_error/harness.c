/* gate: hermes_state_py__is_malformed_schema_error — traced copy of is_malformed_schema_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_malformed_schema_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_malformed_schema_error");
    bool result = is_malformed_schema_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_malformed_schema_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_malformed_schema_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
