/* gate: tools_checkpoint_manager_py___validate_commit_hash — traced copy of _validate_commit_hash */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _validate_commit_hash(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_validate_commit_hash");
    bool result = _validate_commit_hash(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_validate_commit_hash");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _validate_commit_hash <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
