/* gate: agent_credential_sources_py__register — traced copy of crs_register */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int crs_register(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("crs_register");
    int result = crs_register(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("crs_register");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s crs_register <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
