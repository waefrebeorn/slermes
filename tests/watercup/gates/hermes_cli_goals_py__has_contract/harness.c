/* gate: hermes_cli_goals_py__has_contract — traced copy of field_index */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int field_index(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("field_index");
    int result = field_index(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("field_index");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s field_index <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
