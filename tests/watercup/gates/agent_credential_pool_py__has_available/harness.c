/* gate: agent_credential_pool_py__has_available — traced copy of cpl_has_available */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cpl_has_available(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cpl_has_available");
    bool result = cpl_has_available(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cpl_has_available");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_has_available <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
