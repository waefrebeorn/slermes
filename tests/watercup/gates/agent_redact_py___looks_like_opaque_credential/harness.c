/* gate: agent_redact_py___looks_like_opaque_credential — traced copy of _looks_like_opaque_credential */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _looks_like_opaque_credential(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_looks_like_opaque_credential");
    int result = _looks_like_opaque_credential(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_looks_like_opaque_credential");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _looks_like_opaque_credential <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
