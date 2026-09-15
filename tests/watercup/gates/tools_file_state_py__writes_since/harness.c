/* gate: tools_file_state_py__writes_since — traced copy of tools_file_state_writes_since */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_file_state_writes_since(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_file_state_writes_since");
    int result = tools_file_state_writes_since(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_file_state_writes_since");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_file_state_writes_since <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
